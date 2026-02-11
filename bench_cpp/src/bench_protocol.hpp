#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace bench {

static constexpr std::size_t kPayloadBytes = 1024;

static constexpr const char* kDefaultReqKey = "demo/zenoh/bench/req";
static constexpr const char* kDefaultAckKey = "demo/zenoh/bench/ack";

#pragma pack(push, 1)
struct ReqHeader {
    std::uint64_t seq;
    std::uint64_t client_send_mono_ns;
};

struct AckHeader {
    std::uint64_t seq;
    std::uint64_t server_recv_mono_ns;
    std::uint64_t server_send_mono_ns;
};
#pragma pack(pop)

static_assert(sizeof(ReqHeader) == 16, "ReqHeader size must be 16 bytes");
static_assert(sizeof(AckHeader) == 24, "AckHeader size must be 24 bytes");

inline std::string make_req_payload(std::uint64_t seq, std::uint64_t client_send_mono_ns) {
    std::string payload(kPayloadBytes, '\0');
    ReqHeader hdr{seq, client_send_mono_ns};
    std::memcpy(payload.data(), &hdr, sizeof(hdr));
    return payload;
}

inline std::string make_ack_payload(std::uint64_t seq,
                                    std::uint64_t server_recv_mono_ns,
                                    std::uint64_t server_send_mono_ns) {
    std::string payload(sizeof(AckHeader), '\0');
    AckHeader hdr{seq, server_recv_mono_ns, server_send_mono_ns};
    std::memcpy(payload.data(), &hdr, sizeof(hdr));
    return payload;
}

inline bool parse_req_payload(const void* data, std::size_t len, ReqHeader& out) {
    if (len < sizeof(ReqHeader)) return false;
    std::memcpy(&out, data, sizeof(ReqHeader));
    return true;
}

inline bool parse_ack_payload(const void* data, std::size_t len, AckHeader& out) {
    if (len < sizeof(AckHeader)) return false;
    std::memcpy(&out, data, sizeof(AckHeader));
    return true;
}

}  // namespace bench

