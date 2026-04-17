#include "VehicleModeSystem.h"

namespace body_ecu::adapters {

VehicleModeSystem::VehicleModeSystem(SomeIpSystem& someip,
                                     const body::VehicleModeConfig& config)
    : manager_(someip, config) {}

void VehicleModeSystem::init() {
    manager_.init();
    transitionDone();
}

void VehicleModeSystem::run() {
    transitionDone();
}

void VehicleModeSystem::shutdown() {
    manager_.setMode(ports::VehicleMode::Off);
    transitionDone();
}

}  // namespace body_ecu::adapters
