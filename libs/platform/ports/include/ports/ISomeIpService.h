#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace body_ecu::ports {

struct SomeIpMessage {
    uint16_t service_id{0};
    uint16_t method_id{0};
    uint16_t client_id{0};
    uint16_t session_id{0};
    uint8_t message_type{0};
    uint8_t return_code{0};
    std::vector<uint8_t> payload;
};

using MethodHandler = std::function<SomeIpMessage(const SomeIpMessage&)>;

class ISomeIpService {
public:
    virtual ~ISomeIpService() = default;

    virtual void registerMethod(uint16_t service_id, uint16_t method_id,
                                MethodHandler handler) = 0;
    virtual void registerEvent(uint16_t service_id, uint16_t event_id,
                               uint16_t eventgroup_id) = 0;
    virtual void sendEvent(uint16_t service_id, uint16_t event_id,
                           const std::vector<uint8_t>& payload) = 0;
    virtual void sendResponse(const SomeIpMessage& response) = 0;
};

}  // namespace body_ecu::ports
