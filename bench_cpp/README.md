# Zenoh 1kHz RTT 压测（C++）

本目录为 **Zenoh** 局域网高频数据压测工具，用于在两台电脑间以 **1kHz** 发送 **1KB** 载荷，并统计 **RTT 延迟**、**到达间距（抖动）**、吞吐与可靠性等指标。全程使用 **RTT 往返测量**，不依赖两台机器的系统时间同步。

## 项目说明

- **目标**：评估 Zenoh 在局域网下 1kHz、1KB 载荷的延迟与抖动表现。
- **实现**：纯 C++，独立 CMake 工程，不修改仓库内 `cpp/`、`python/` 示例。
- **流程**：发布端按固定频率发送请求（req），订阅端收到后立即回 ACK；发布端用本机时钟测量「发送时刻 → 收到 ACK 时刻」的 RTT。

### 可执行文件

| 程序 | 作用 |
|------|------|
| `bench_echo_ack` | 订阅请求 key，收到后立即发布 ACK；统计到达间距、吞吐、乱序。 |
| `bench_pub_rtt` | 按 1kHz 发送请求，订阅 ACK，统计 RTT（含百分位）、超时、吞吐。 |

### 默认 Key

- 请求：`demo/zenoh/bench/req`
- ACK：`demo/zenoh/bench/ack`

可通过 `--req-key`、`--ack-key` 覆盖。

---

## 环境要求

- CMake ≥ 3.10，C++17
- 已安装 Zenoh C/C++ 库（`libzenohc`、`libzenohcpp`），与仓库根目录 C++ 示例相同
- 运行压测时需有可达的 **zenoh 路由器**（如 `zenohd`），通常监听 `tcp/0.0.0.0:7447`

---

## 操作步骤

### 1. 构建

在仓库根目录执行：

```bash
cmake -S bench_cpp -B build-bench
cmake --build build-bench -j
```

生成的可执行文件位于 `build-bench/`：

- `build-bench/bench_echo_ack`
- `build-bench/bench_pub_rtt`

### 2. 启动 zenoh 路由器（若尚未运行）

例如在一台机器上启动：

```bash
zenohd
```

默认会监听 `tcp/7447`。两台电脑压测时，确保发送端、接收端都能连到该路由器（将下面 `--connect` 中的 IP 改为路由器或对端 IP）。

### 3. 先启动接收端（回 ACK + 统计到达间距）

在**接收端机器**（或本机同机测试）运行：

```bash
./build-bench/bench_echo_ack --connect tcp/127.0.0.1:7447
```

局域网时改为路由器/对端 IP，例如：

```bash
./build-bench/bench_echo_ack --connect tcp/192.168.1.100:7447
```

### 4. 再启动发送端（1kHz 发送 + RTT 统计）

在**发送端机器**运行，推荐用 `--count` 做固定条数测试：

```bash
./build-bench/bench_pub_rtt --connect tcp/127.0.0.1:7447 --rate-hz 1000 --count 100000 --ack-timeout-ms 100
```

或按时长（默认 10 秒）：

```bash
./build-bench/bench_pub_rtt --connect tcp/192.168.1.100:7447 --rate-hz 1000 --duration-sec 10 --ack-timeout-ms 100
```

### 5. 结束与查看结果

- 发送端在发完 `--count` 条或到达 `--duration-sec` 后会自动打印 summary 并退出（也可 Ctrl+C 提前结束）。
- 接收端需在发送端结束后按 **Ctrl+C** 退出，退出时会打印 summary。

两端的 summary 均为一行 `key=value` 格式，便于脚本解析或人工查看。

---

## 常用参数

### bench_echo_ack

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--connect` | Zenoh 端点（如 `tcp/IP:7447`） | `tcp/127.0.0.1:7447` |
| `--req-key` | 请求 key | `demo/zenoh/bench/req` |
| `--ack-key` | ACK key | `demo/zenoh/bench/ack` |
| `--quiet` | 关闭每千条打印 | 否 |

### bench_pub_rtt

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--connect` | Zenoh 端点 | `tcp/127.0.0.1:7447` |
| `--req-key` | 请求 key | `demo/zenoh/bench/req` |
| `--ack-key` | ACK key | `demo/zenoh/bench/ack` |
| `--rate-hz` | 发送频率（Hz） | 1000 |
| `--payload-bytes` | 载荷字节数（必须 1024） | 1024 |
| `--count` | 发送总条数（设则忽略 `--duration-sec`） | 0 |
| `--duration-sec` | 发送时长（秒），`--count` 未设时生效 | 10.0 |
| `--ack-timeout-ms` | ACK 超时（毫秒），超时计为 timeouts | 100 |
| `--quiet` | 减少进度日志 | 否 |

---

## 指标解读

### 发送端（bench_pub_rtt）summary 示例

```
summary duration_sec=100.5 sent=100000 ack_received=99800 timeouts=200 out_of_order=0 pending_inflight=0 sent_per_sec=995.02 ack_per_sec=993.03 mb_per_sec=0.99 rtt_us_avg=450.2 rtt_us_min=320.1 rtt_us_max=2100.5 rtt_us_p50=420.0 rtt_us_p95=680.0 rtt_us_p99=1200.0 rtt_us_stddev=85.3
```

| 指标 | 含义 |
|------|------|
| `duration_sec` | 从开始发送到结束的时长（秒）。 |
| `sent` | 实际发送的请求条数。 |
| `ack_received` | 在超时前收到的 ACK 条数，即有效 RTT 样本数。 |
| `timeouts` | 在 `--ack-timeout-ms` 内未收到 ACK 的条数，可视为丢包或严重延迟。 |
| `out_of_order` | 收到的 ACK 序列号小于等于上一条的次数（乱序）。 |
| `pending_inflight` | 结束时仍未收到 ACK（且未计为 timeout）的条数，正常应为 0 或很小。 |
| `sent_per_sec` | 发送速率（条/秒），应接近 `--rate-hz`。 |
| `ack_per_sec` | 有效 ACK 速率（条/秒）。 |
| `mb_per_sec` | 按 1KB/条计算的发送带宽（MB/s）。 |
| `rtt_us_avg` | RTT 平均值（微秒）。 |
| `rtt_us_min` / `rtt_us_max` | RTT 最小/最大值（微秒）。 |
| `rtt_us_p50` / `rtt_us_p95` / `rtt_us_p99` | RTT 的 50%/95%/99% 分位（微秒），用于看长尾延迟。 |
| `rtt_us_stddev` | RTT 标准差（微秒），反映 RTT 波动。 |

**注意**：RTT 为「请求发出 → 收到对应 ACK」的往返时间，全部在发送端本机用单调时钟测量，**不依赖两台电脑系统时间是否一致**。

### 接收端（bench_echo_ack）summary 示例

```
summary duration_sec=100.5 recv=99800 msg_per_sec=993.03 mb_per_sec=0.99 out_of_order=0 interarrival_us_avg=1007.2 interarrival_us_min=800.1 interarrival_us_max=2500.0 interarrival_us_stddev=120.5
```

| 指标 | 含义 |
|------|------|
| `duration_sec` | 从首次收到请求到进程退出的时长。 |
| `recv` | 收到的请求条数（即发出的 ACK 条数）。 |
| `msg_per_sec` | 接收速率（条/秒）。 |
| `mb_per_sec` | 按 1KB/条计算的接收带宽（MB/s）。 |
| `out_of_order` | 请求序列号小于等于上一条的次数。 |
| `interarrival_us_avg` | 相邻两条请求到达时间间隔的平均值（微秒），目标 1kHz 时理想约 1000 μs。 |
| `interarrival_us_min` / `interarrival_us_max` | 到达间隔的最小/最大值。 |
| `interarrival_us_stddev` | 到达间隔的标准差，反映**抖动**大小。 |

到达间隔全部在接收端用本机单调时钟计算，不受两机系统时间偏差影响。

---

## 简要结论建议

- **延迟**：主要看发送端 `rtt_us_avg`、`rtt_us_p95`/`rtt_us_p99`；若需粗略估计单向延迟，可参考 `RTT/2`（假设链路大致对称）。
- **抖动**：看接收端 `interarrival_us_stddev`、`interarrival_us_max`；发送端可看 `rtt_us_stddev`。
- **可靠性**：`timeouts` 应尽量为 0 或很小；`ack_received` 应接近 `sent`；`out_of_order` 多则说明存在乱序。
- **吞吐**：`sent_per_sec`/`ack_per_sec` 接近 `--rate-hz`、`mb_per_sec` 约 1 MB/s（1kHz × 1KB）即达标。
