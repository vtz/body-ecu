#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace body_ecu::platform {

namespace doip {

constexpr uint8_t  kProtocolVersion    = 0x03;   // ISO 13400-2:2019
constexpr uint8_t  kInverseVersion     = static_cast<uint8_t>(~kProtocolVersion);
constexpr uint16_t kDefaultPort        = 13400;
constexpr size_t   kHeaderLen          = 8;

enum class PayloadType : uint16_t {
    VehicleIdentRequest          = 0x0001,
    VehicleIdentResponse         = 0x0004,
    RoutingActivationRequest     = 0x0005,
    RoutingActivationResponse    = 0x0006,
    AliveCheckRequest            = 0x0007,
    AliveCheckResponse           = 0x0008,
    DiagnosticMessage            = 0x8001,
    DiagnosticPositiveAck        = 0x8002,
    DiagnosticNegativeAck        = 0x8003,
    GenericNack                  = 0x0000,
};

enum class RoutingActivationCode : uint8_t {
    Success                 = 0x10,
    UnknownSourceAddress    = 0x00,
    AlreadyActive           = 0x02,
};

struct Header {
    uint8_t  version{kProtocolVersion};
    uint8_t  inverse{kInverseVersion};
    uint16_t payload_type{0};
    uint32_t payload_length{0};
};

inline Header parseHeader(const uint8_t* data) {
    Header h;
    h.version        = data[0];
    h.inverse        = data[1];
    h.payload_type   = static_cast<uint16_t>((data[2] << 8) | data[3]);
    h.payload_length = static_cast<uint32_t>(
        (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7]);
    return h;
}

inline std::vector<uint8_t> serializeHeader(PayloadType type,
                                             uint32_t payload_len) {
    return {
        kProtocolVersion,
        kInverseVersion,
        static_cast<uint8_t>(static_cast<uint16_t>(type) >> 8),
        static_cast<uint8_t>(static_cast<uint16_t>(type) & 0xFF),
        static_cast<uint8_t>((payload_len >> 24) & 0xFF),
        static_cast<uint8_t>((payload_len >> 16) & 0xFF),
        static_cast<uint8_t>((payload_len >>  8) & 0xFF),
        static_cast<uint8_t>((payload_len      ) & 0xFF),
    };
}

inline std::vector<uint8_t> makeRoutingActivationResponse(
    uint16_t tester_addr, uint16_t entity_addr, RoutingActivationCode code) {
    // payload: tester addr (2) + entity addr (2) + response code (1) + reserved (4)
    constexpr uint32_t kPayloadLen = 9;
    auto msg = serializeHeader(PayloadType::RoutingActivationResponse, kPayloadLen);
    msg.push_back(static_cast<uint8_t>(tester_addr >> 8));
    msg.push_back(static_cast<uint8_t>(tester_addr & 0xFF));
    msg.push_back(static_cast<uint8_t>(entity_addr >> 8));
    msg.push_back(static_cast<uint8_t>(entity_addr & 0xFF));
    msg.push_back(static_cast<uint8_t>(code));
    msg.insert(msg.end(), 4, 0x00);   // reserved / OEM
    return msg;
}

inline std::vector<uint8_t> makeDiagnosticAck(
    uint16_t source_addr, uint16_t target_addr, uint8_t ack_code) {
    constexpr uint32_t kPayloadLen = 5;
    auto msg = serializeHeader(PayloadType::DiagnosticPositiveAck, kPayloadLen);
    msg.push_back(static_cast<uint8_t>(source_addr >> 8));
    msg.push_back(static_cast<uint8_t>(source_addr & 0xFF));
    msg.push_back(static_cast<uint8_t>(target_addr >> 8));
    msg.push_back(static_cast<uint8_t>(target_addr & 0xFF));
    msg.push_back(ack_code);
    return msg;
}

inline std::vector<uint8_t> makeDiagnosticMessage(
    uint16_t source_addr, uint16_t target_addr,
    const std::vector<uint8_t>& uds_payload) {
    uint32_t payload_len = 4 + static_cast<uint32_t>(uds_payload.size());
    auto msg = serializeHeader(PayloadType::DiagnosticMessage, payload_len);
    msg.push_back(static_cast<uint8_t>(source_addr >> 8));
    msg.push_back(static_cast<uint8_t>(source_addr & 0xFF));
    msg.push_back(static_cast<uint8_t>(target_addr >> 8));
    msg.push_back(static_cast<uint8_t>(target_addr & 0xFF));
    msg.insert(msg.end(), uds_payload.begin(), uds_payload.end());
    return msg;
}

}  // namespace doip
}  // namespace body_ecu::platform
