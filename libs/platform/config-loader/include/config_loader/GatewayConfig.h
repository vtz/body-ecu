#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace body_ecu::config {

enum class MappingDirection { SomeIpToCan, CanToSomeIp };

struct SomeIpRef {
    uint16_t service_id{0};
    uint16_t method_id{0};
    uint16_t event_id{0};
    uint16_t eventgroup_id{0};
};

struct CanRef {
    uint32_t id{0};
    uint8_t dlc{0};
};

struct GatewayMapping {
    std::string name;
    MappingDirection direction{MappingDirection::SomeIpToCan};
    SomeIpRef someip;
    CanRef can;
};

struct GatewayConfig {
    std::vector<GatewayMapping> mappings;
};

}  // namespace body_ecu::config
