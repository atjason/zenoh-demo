#include "bench_protocol.hpp"
#include "zenoh.hxx"

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>

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
        double delta = x - mean;
        mean += delta / static_cast<double>(n);
        double delta2 = x - mean;
        m2 += delta * delta2;
    }

    double variance() const { return (n >= 2) ? (m2 / static_cast<double>(n - 1)) : 0.0; }
    double stddev() const { return std::sqrt(variance()); }
};

struct Args {
    std::string connect = "tcp/127.0.0.1:7447";
    std::string req_key = bench::kDefaultReqKey;
    std::string ack_key = bench::kDefaultAckKey;
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
        } else if (a == "--quiet") {
            out.quiet = true;
        } else if (a == "-h" || a == "--help") {
            std::cout
                << "bench_echo_ack\n\n"
                << "  --connect  <endpoint>   (default: tcp/127.0.0.1:7447)\n"
                << "  --req-key   <keyexpr>   (default: " << bench::kDefaultReqKey << ")\n"
                << "  --ack-key   <keyexpr>   (default: " << bench::kDefaultAckKey << ")\n"
                << "  --quiet                (disable per-message logs)\n";
            std::exit(0);
        } else {
            std::cerr << "Unknown arg: " << a << "\n";
            return false;
        }
    }
    return true;
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

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    try {
        Config config = Config::create_default();
        const std::string endpoints_json = "[\"" + args.connect + "\"]";
        config.insert_json5("connect/endpoints", endpoints_json);
        auto session = Session::open(std::move(config));

        auto ack_pub = session.declare_publisher(KeyExpr(args.ack_key));

        std::cout << "bench_echo_ack connected=" << args.connect << " req_key=" << args.req_key
                  << " ack_key=" << args.ack_key << "\n";

        using Clock = std::chrono::steady_clock;
        bool have_prev = false;
        Clock::time_point prev_tp{};
        OnlineStats interarrival_us{};
        std::uint64_t recv_count = 0;
        std::uint64_t out_of_order = 0;
        std::uint64_t last_seq = 0;
        bool have_last_seq = false;
        std::mutex mu;

        const auto start_tp = Clock::now();

        auto sub = session.declare_subscriber(
            KeyExpr(args.req_key),
            [&](const Sample& sample) {
                const auto now_tp = Clock::now();

                std::string payload = sample.get_payload().as_string();
                bench::ReqHeader req{};
                if (!bench::parse_req_payload(payload.data(), payload.size(), req)) {
                    if (!args.quiet) {
                        std::cerr << "Failed to parse req payload (len=" << payload.size() << ")\n";
                    }
                    return;
                }

                {
                    std::lock_guard<std::mutex> lk(mu);
                    ++recv_count;
                    if (have_prev) {
                        const auto dt =
                            std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(now_tp - prev_tp);
                        interarrival_us.add(dt.count());
                    } else {
                        have_prev = true;
                    }
                    prev_tp = now_tp;

                    if (have_last_seq && req.seq <= last_seq) ++out_of_order;
                    last_seq = req.seq;
                    have_last_seq = true;
                }

                const std::uint64_t srv_recv_ns = steady_now_ns();
                const std::uint64_t srv_send_ns = steady_now_ns();
                std::string ack = bench::make_ack_payload(req.seq, srv_recv_ns, srv_send_ns);
                ack_pub.put(ack);

                if (!args.quiet && (req.seq % 1000 == 0)) {
                    std::uint64_t total = 0;
                    {
                        std::lock_guard<std::mutex> lk(mu);
                        total = recv_count;
                    }
                    std::cout << "recv seq=" << req.seq << " total=" << total << "\n";
                }
            },
            closures::none);

        (void)sub;

        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        const auto end_tp = Clock::now();
        const double dur_s =
            std::chrono::duration_cast<std::chrono::duration<double>>(end_tp - start_tp).count();
        std::uint64_t recv_count_snapshot = 0;
        std::uint64_t out_of_order_snapshot = 0;
        OnlineStats interarrival_snapshot{};
        {
            std::lock_guard<std::mutex> lk(mu);
            recv_count_snapshot = recv_count;
            out_of_order_snapshot = out_of_order;
            interarrival_snapshot = interarrival_us;
        }

        const double msg_per_s =
            (dur_s > 0.0) ? (static_cast<double>(recv_count_snapshot) / dur_s) : 0.0;
        const double mb_per_s =
            (dur_s > 0.0)
                ? ((static_cast<double>(recv_count_snapshot) * bench::kPayloadBytes) / dur_s / 1024.0 / 1024.0)
                : 0.0;

        std::cout << "summary "
                  << "duration_sec=" << dur_s << " recv=" << recv_count_snapshot << " msg_per_sec=" << msg_per_s
                  << " mb_per_sec=" << mb_per_s << " out_of_order=" << out_of_order_snapshot;

        if (interarrival_snapshot.n > 0) {
            std::cout << " interarrival_us_avg=" << interarrival_snapshot.mean
                      << " interarrival_us_min=" << interarrival_snapshot.min_v
                      << " interarrival_us_max=" << interarrival_snapshot.max_v
                      << " interarrival_us_stddev=" << interarrival_snapshot.stddev();
        }
        std::cout << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error in bench_echo_ack: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

