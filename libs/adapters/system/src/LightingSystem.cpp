#include "LightingSystem.h"

namespace body_ecu::adapters {

LightingSystem::LightingSystem(ports::IGpioPort& gpio, SomeIpSystem& someip,
                               const body::LightingConfig& config)
    : controller_(gpio, someip, config) {}

void LightingSystem::init() {
    controller_.init();
}

void LightingSystem::run() {
    // Controller is event-driven via SOME/IP method handlers;
    // no periodic work needed.
}

void LightingSystem::shutdown() {
    // Turn off all lights on shutdown
    for (size_t i = 0; i < body::kLightCount; ++i) {
        controller_.setLightState(static_cast<body::LightId>(i), false);
    }
}

}  // namespace body_ecu::adapters
