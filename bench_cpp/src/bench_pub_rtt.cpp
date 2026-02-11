#include "bench_protocol.hpp"
#include "zenoh.hxx"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace zenoh;

namespace {

struct OnlineStats {
    std::uint64_t n = 0;
    double mean = 0.0;
    double m2 = 0.0;
    double min_v = std::numeric_limits<double>::infinity();
    double max_v = -std::numeric_limits<double>::infinity();

    void add(double x) {
        ++n;
        if (x < min_v) min_v = x;
        if (x > max_v) max_v = x;
        const double delta = x - mean;
        mean += delta / static_cast<double>(n);
        const double delta2 = x - mean;
        m2 += delta * delta2;
    }

    double variance() const { return (n >= 2) ? (m2 / static_cast<double>(n - 1)) : 0.0; }
    double stddev() const { return std::sqrt(variance()); }
};

struct Args {
    std::string connect = "tcp/127.0.0.1:7447";
    std::string req_key = bench::kDefaultReqKey;
    std::string ack_key = bench::kDefaultAckKey;
    int rate_hz = 1000;
    std::size_t payload_bytes = bench::kPayloadBytes;  // default 1024

    // End condition:
    std::uint64_t count = 0;       // if >0: send exactly this many
    double duration_sec = 10.0;    // used if count==0

    int ack_timeout_ms = 100;
    bool quiet = false;
};

bool parse_args(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (a == "--connect") {
            const char* v = need("--connect");
            if (!v) return false;
            out.connect = v;
        } else if (a == "--req-key") {
            const char* v = need("--req-key");
            if (!v) return false;
            out.req_key = v;
        } else if (a == "--ack-key") {
            const char* v = need("--ack-key");
            if (!v) return false;
            out.ack_key = v;
        } else if (a == "--rate-hz") {
            const char* v = need("--rate-hz");
            if (!v) return false;
            out.rate_hz = std::atoi(v);
        } else if (a == "--payload-bytes") {
            const char* v = need("--payload-bytes");
            if (!v) return false;
            out.payload_bytes = static_cast<std::size_t>(std::strtoull(v, nullptr, 10));
        } else if (a == "--count") {
            const char* v = need("--count");
            if (!v) return false;
            out.count = static_cast<std::uint64_t>(std::strtoull(v, nullptr, 10));
        } else if (a == "--duration-sec") {
            const char* v = need("--duration-sec");
            if (!v) return false;
            out.duration_sec = std::atof(v);
        } else if (a == "--ack-timeout-ms") {
            const char* v = need("--ack-timeout-ms");
            if (!v) return false;
            out.ack_timeout_ms = std::atoi(v);
        } else if (a == "--quiet") {
            out.quiet = true;
        } else if (a == "-h" || a == "--help") {
            std::cout
                << "bench_pub_rtt\n\n"
                << "  --connect         <endpoint>  (default: tcp/127.0.0.1:7447)\n"
                << "  --req-key         <keyexpr>   (default: " << bench::kDefaultReqKey << ")\n"
                << "  --ack-key         <keyexpr>   (default: " << bench::kDefaultAckKey << ")\n"
                << "  --rate-hz         <int>       (default: 1000)\n"
                << "  --payload-bytes   <int>       (default: " << bench::kPayloadBytes
                << ", must be >= " << sizeof(bench::ReqHeader) << ")\n"
                << "  --count           <uint64>    (if set, ignore --duration-sec)\n"
                << "  --duration-sec    <double>    (default: 10.0)\n"
                << "  --ack-timeout-ms  <int>       (default: 100)\n"
                << "  --quiet                      (reduce logs)\n";
            std::exit(0);
        } else {
            std::cerr << "Unknown arg: " << a << "\n";
            return false;
        }
    }
    return true;
}

template <class T>
double percentile_sorted(const std::vector<T>& sorted, double p01) {
    if (sorted.empty()) return 0.0;
    if (p01 <= 0.0) return static_cast<double>(sorted.front());
    if (p01 >= 1.0) return static_cast<double>(sorted.back());
    const double idx = p01 * static_cast<double>(sorted.size() - 1);
    const std::size_t i = static_cast<std::size_t>(idx);
    return static_cast<double>(sorted[i]);
}

std::uint64_t steady_now_ns() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::atomic<bool> g_running{true};
void handle_signal(int) { g_running.store(false); }

}  // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, args)) return 2;

    if (args.rate_hz <= 0) {
        std::cerr << "--rate-hz must be > 0\n";
        return 2;
    }
    if (args.payload_bytes < sizeof(bench::ReqHeader)) {
        std::cerr << "--payload-bytes must be >= " << sizeof(bench::ReqHeader) << "\n";
        return 2;
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    try {
        Config config = Config::create_default();
        const std::string endpoints_json = "[\"" + args.connect + "\"]";
        config.insert_json5("connect/endpoints", endpoints_json);
        auto session = Session::open(std::move(config));

        auto req_pub = session.declare_publisher(KeyExpr(args.req_key));

        using Clock = std::chrono::steady_clock;
        using TP = Clock::time_point;

        std::mutex mu;
        std::vector<TP> send_ts;          // indexed by seq when count mode
        std::vector<std::uint8_t> state;  // 0=unsent,1=inflight,2=acked,3=timedout
        std::deque<std::uint64_t> inflight;
        std::unordered_map<std::uint64_t, TP> send_map;  // duration mode
        std::vector<double> rtt_us_samples;
        OnlineStats rtt_us_stats;
        std::uint64_t ack_received = 0;
        std::uint64_t timeouts = 0;
        std::uint64_t out_of_order = 0;
        std::uint64_t last_ack_seq = 0;
        bool have_last_ack_seq = false;

        const auto timeout = std::chrono::milliseconds(args.ack_timeout_ms);

        if (args.count > 0) {
            send_ts.resize(static_cast<std::size_t>(args.count));
            state.resize(static_cast<std::size_t>(args.count), 0);
            rtt_us_samples.reserve(static_cast<std::size_t>(args.count));
        } else {
            rtt_us_samples.reserve(static_cast<std::size_t>(args.rate_hz * args.duration_sec * 1.2));
            send_map.reserve(static_cast<std::size_t>(args.rate_hz * 2));
        }

        auto ack_sub = session.declare_subscriber(
            KeyExpr(args.ack_key),
            [&](const Sample& sample) {
                const auto now_tp = Clock::now();
                std::string payload = sample.get_payload().as_string();
                bench::AckHeader ack{};
                if (!bench::parse_ack_payload(payload.data(), payload.size(), ack)) return;

                std::lock_guard<std::mutex> lk(mu);

                if (have_last_ack_seq && ack.seq <= last_ack_seq) ++out_of_order;
                last_ack_seq = ack.seq;
                have_last_ack_seq = true;

                if (args.count > 0) {
                    if (ack.seq >= args.count) return;
                    if (state[static_cast<std::size_t>(ack.seq)] != 1) return;  // not inflight
                    const auto sent_tp = send_ts[static_cast<std::size_t>(ack.seq)];
                    const auto rtt = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(now_tp - sent_tp);
                    state[static_cast<std::size_t>(ack.seq)] = 2;
                    ++ack_received;
                    rtt_us_stats.add(rtt.count());
                    rtt_us_samples.push_back(rtt.count());
                } else {
                    auto it = send_map.find(ack.seq);
                    if (it == send_map.end()) return;
                    const auto rtt =
                        std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(now_tp - it->second);
                    send_map.erase(it);
                    ++ack_received;
                    rtt_us_stats.add(rtt.count());
                    rtt_us_samples.push_back(rtt.count());
                }
            },
            closures::none);

        (void)ack_sub;

        std::cout << "bench_pub_rtt connected=" << args.connect << " req_key=" << args.req_key
                  << " ack_key=" << args.ack_key << " rate_hz=" << args.rate_hz
                  << " payload_bytes=" << args.payload_bytes
                  << ((args.count > 0) ? (" count=" + std::to_string(args.count))
                                       : (" duration_sec=" + std::to_string(args.duration_sec)))
                  << " ack_timeout_ms=" << args.ack_timeout_ms << "\n";

        const auto start_tp = Clock::now();
        const auto interval = std::chrono::microseconds(static_cast<int>(1000000 / args.rate_hz));
        auto next_send = start_tp;

        std::uint64_t sent = 0;
        auto should_continue = [&]() -> bool {
            if (!g_running.load()) return false;
            if (args.count > 0) return sent < args.count;
            const auto now = Clock::now();
            const double elapsed =
                std::chrono::duration_cast<std::chrono::duration<double>>(now - start_tp).count();
            return elapsed < args.duration_sec;
        };

        while (should_continue()) {
            const auto now_tp = Clock::now();
            if (now_tp < next_send) {
                std::this_thread::sleep_until(next_send);
            }

            const auto send_tp = Clock::now();
            const std::uint64_t send_ns = steady_now_ns();
            const std::uint64_t seq = sent++;

            if (args.count > 0) {
                std::lock_guard<std::mutex> lk(mu);
                send_ts[static_cast<std::size_t>(seq)] = send_tp;
                state[static_cast<std::size_t>(seq)] = 1;
                inflight.push_back(seq);
            } else {
                std::lock_guard<std::mutex> lk(mu);
                send_map.emplace(seq, send_tp);
                inflight.push_back(seq);
            }

            std::string payload = bench::make_req_payload(seq, send_ns, args.payload_bytes);
            req_pub.put(payload);

            if (!args.quiet && (seq % 1000 == 0)) {
                std::size_t inflight_sz = 0;
                {
                    std::lock_guard<std::mutex> lk(mu);
                    inflight_sz = inflight.size();
                }
                std::cout << "sent seq=" << seq << " inflight=" << inflight_sz << "\n";
            }

            // Timeout processing (count mode)
            if (args.ack_timeout_ms > 0) {
                std::lock_guard<std::mutex> lk(mu);
                const auto now2 = Clock::now();
                while (!inflight.empty()) {
                    const std::uint64_t s = inflight.front();
                    if (args.count > 0) {
                        const auto st = state[static_cast<std::size_t>(s)];
                        if (st == 2 || st == 3) {
                            inflight.pop_front();
                            continue;
                        }
                        const auto age = now2 - send_ts[static_cast<std::size_t>(s)];
                        if (age > timeout) {
                            state[static_cast<std::size_t>(s)] = 3;
                            ++timeouts;
                            inflight.pop_front();
                            continue;
                        }
                        break;
                    } else {
                        auto it = send_map.find(s);
                        if (it == send_map.end()) {
                            inflight.pop_front();
                            continue;
                        }
                        const auto age = now2 - it->second;
                        if (age > timeout) {
                            send_map.erase(it);
                            ++timeouts;
                            inflight.pop_front();
                            continue;
                        }
                        break;
                    }
                }
            }

            next_send += interval;
        }

        // Drain remaining inflight until timeout threshold reached.
        if (args.ack_timeout_ms > 0) {
            const auto drain_until = Clock::now() + timeout;
            while (Clock::now() < drain_until) {
                {
                    std::lock_guard<std::mutex> lk(mu);
                    const auto now_tp = Clock::now();
                    while (!inflight.empty()) {
                        const std::uint64_t s = inflight.front();
                        if (args.count > 0) {
                            const auto st = state[static_cast<std::size_t>(s)];
                            if (st == 2 || st == 3) {
                                inflight.pop_front();
                                continue;
                            }
                            const auto age = now_tp - send_ts[static_cast<std::size_t>(s)];
                            if (age > timeout) {
                                state[static_cast<std::size_t>(s)] = 3;
                                ++timeouts;
                                inflight.pop_front();
                                continue;
                            }
                            break;
                        } else {
                            auto it = send_map.find(s);
                            if (it == send_map.end()) {
                                inflight.pop_front();
                                continue;
                            }
                            const auto age = now_tp - it->second;
                            if (age > timeout) {
                                send_map.erase(it);
                                ++timeouts;
                                inflight.pop_front();
                                continue;
                            }
                            break;
                        }
                    }
                    if (inflight.empty()) break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        const auto end_tp = Clock::now();
        const double dur_s =
            std::chrono::duration_cast<std::chrono::duration<double>>(end_tp - start_tp).count();
        const double sent_per_s = (dur_s > 0.0) ? (static_cast<double>(sent) / dur_s) : 0.0;
        const double ack_per_s = (dur_s > 0.0) ? (static_cast<double>(ack_received) / dur_s) : 0.0;
        const double mb_per_s =
            (dur_s > 0.0) ? ((static_cast<double>(sent) * args.payload_bytes) / dur_s / 1024.0 / 1024.0)
                          : 0.0;

        std::uint64_t pending_inflight = 0;
        {
            std::lock_guard<std::mutex> lk(mu);
            if (args.count > 0) {
                for (std::size_t i = 0; i < state.size(); ++i) {
                    if (state[i] == 1) ++pending_inflight;
                }
            } else {
                pending_inflight = static_cast<std::uint64_t>(send_map.size());
            }
        }

        std::vector<double> sorted = rtt_us_samples;
        std::sort(sorted.begin(), sorted.end());

        const double p50 = percentile_sorted(sorted, 0.50);
        const double p95 = percentile_sorted(sorted, 0.95);
        const double p99 = percentile_sorted(sorted, 0.99);

        const auto old_flags = std::cout.flags();
        const auto old_prec = std::cout.precision();
        std::cout.setf(std::ios::fixed);
        std::cout << std::setprecision(3);

        std::cout << "=== 汇总（RTT 往返时延测试）===\n"
                  << "运行时长: " << dur_s << " 秒\n"
                  << "发送请求: " << sent << " 条\n"
                  << "收到 ACK: " << ack_received << " 条\n"
                  << "超时次数: " << timeouts << " 条\n"
                  << "乱序 ACK: " << out_of_order << " 条\n"
                  << "在途未完成: " << pending_inflight << " 条\n"
                  << "发送速率: " << sent_per_s << " 条/秒\n"
                  << "ACK 速率: " << ack_per_s << " 条/秒\n"
                  << "吞吐量: " << mb_per_s << " MiB/秒（payload=" << args.payload_bytes << " 字节）\n";

        if (rtt_us_stats.n > 0) {
            std::cout << "RTT（微秒 us）: 平均 " << rtt_us_stats.mean << "，最小 " << rtt_us_stats.min_v
                      << "，最大 " << rtt_us_stats.max_v << "（约 " << (rtt_us_stats.max_v / 1000.0) << " ms）\n"
                      << "RTT 分位数（微秒 us）: P50 " << p50 << "，P95 " << p95 << "，P99 " << p99 << "\n"
                      << "RTT 抖动（标准差，微秒 us）: " << rtt_us_stats.stddev() << "\n";
        } else {
            std::cout << "RTT: 无有效样本\n";
        }

        std::cout.flags(old_flags);
        std::cout.precision(old_prec);
    } catch (const std::exception& e) {
        std::cerr << "Error in bench_pub_rtt: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

