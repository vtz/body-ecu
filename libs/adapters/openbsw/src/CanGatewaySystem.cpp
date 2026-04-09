#include "CanGatewaySystem.h"

namespace body_ecu::adapters {

CanGatewaySystem::CanGatewaySystem(ports::ICanBus& can, SomeIpSystem& someip)
    : gateway_(can, someip) {}

void CanGatewaySystem::addMapping(const platform::ServiceMapping& mapping) {
    gateway_.addMapping(mapping);
}

void CanGatewaySystem::init() {
    // Mappings should be loaded from config before init.
}

void CanGatewaySystem::run() {
    gateway_.start();
}

void CanGatewaySystem::shutdown() {
    gateway_.stop();
}

}  // namespace body_ecu::adapters
