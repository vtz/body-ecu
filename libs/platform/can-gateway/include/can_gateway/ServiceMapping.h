#pragma once

#include <cstdint>
#include <string>

namespace body_ecu::platform {

enum class GatewayDirection { SomeIpToCan, CanToSomeIp };

struct ServiceMapping {
    std::string name;
    GatewayDirection direction{GatewayDirection::SomeIpToCan};
    uint16_t someip_service_id{0};
    uint16_t someip_method_id{0};
    uint16_t someip_event_id{0};
    uint16_t someip_eventgroup_id{0};
    uint32_t can_id{0};
    uint8_t can_dlc{0};
};

}  // namespace body_ecu::platform
