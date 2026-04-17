#include "CanGatewaySystem.h"

namespace body_ecu::adapters {

CanGatewaySystem::CanGatewaySystem(ports::ICanBus& can, SomeIpSystem& someip)
    : gateway_(can, someip) {}

void CanGatewaySystem::addMapping(const platform::ServiceMapping& mapping) {
    gateway_.addMapping(mapping);
}

void CanGatewaySystem::init() {
    transitionDone();
}

void CanGatewaySystem::run() {
    gateway_.start();
    transitionDone();
}

void CanGatewaySystem::shutdown() {
    gateway_.stop();
    transitionDone();
}

}  // namespace body_ecu::adapters
