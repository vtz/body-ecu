#include "VehicleModeSystem.h"

namespace body_ecu::adapters {

VehicleModeSystem::VehicleModeSystem(SomeIpSystem& someip,
                                     const body::VehicleModeConfig& config)
    : manager_(someip, config) {}

void VehicleModeSystem::init() {
    manager_.init();
}

void VehicleModeSystem::run() {
    // Manager is event-driven via SOME/IP field handlers.
}

void VehicleModeSystem::shutdown() {
    manager_.setMode(ports::VehicleMode::Off);
}

}  // namespace body_ecu::adapters
