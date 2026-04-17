#include "LightingSystem.h"

namespace body_ecu::adapters {

LightingSystem::LightingSystem(ports::IGpioPort& gpio, SomeIpSystem& someip,
                               const body::LightingConfig& config)
    : controller_(gpio, someip, config) {}

void LightingSystem::init() {
    controller_.init();
    transitionDone();
}

void LightingSystem::run() {
    transitionDone();
}

void LightingSystem::shutdown() {
    for (size_t i = 0; i < body::kLightCount; ++i) {
        controller_.setLightState(static_cast<body::LightId>(i), false);
    }
    transitionDone();
}

}  // namespace body_ecu::adapters
