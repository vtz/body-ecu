#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace body_ecu::config {

struct MethodConfig {
    std::string name;
    uint16_t id{0};
};

struct EventConfig {
    std::string name;
    uint16_t id{0};
};

struct EventGroupConfig {
    std::string name;
    uint16_t id{0};
};

struct FieldConfig {
    std::string name;
    uint16_t getter{0};
    uint16_t setter{0};
    uint16_t notifier{0};
};

struct ServiceDescriptor {
    std::string name;
    uint16_t service_id{0};
    uint16_t instance_id{0};
    std::map<std::string, MethodConfig> methods;
    std::map<std::string, EventConfig> events;
    std::map<std::string, EventGroupConfig> eventgroups;
    std::map<std::string, FieldConfig> fields;
};

struct SdConfig {
    std::string multicast_address{"239.255.255.251"};
    uint16_t multicast_port{30490};
    uint32_t cyclic_offer_delay_ms{5000};
};

struct NetworkConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{30490};
    SdConfig sd;
};

struct ServicesConfig {
    NetworkConfig network;
    std::map<std::string, ServiceDescriptor> services;
};

}  // namespace body_ecu::config
