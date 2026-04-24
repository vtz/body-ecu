#include "IgnitionSystem.h"

namespace body_ecu::adapters {

IgnitionSystem::IgnitionSystem(ports::IButtonInput& button,
                               VehicleModeSystem& vehicle_mode,
                               ports::ITimerService& timer,
                               const body::IgnitionConfig& config,
                               ports::ISignalBus* signal_bus)
    : controller_(button, vehicle_mode.manager(), timer, config, signal_bus) {}

void IgnitionSystem::init() {
    controller_.init();
    transitionDone();
}

void IgnitionSystem::run() {
    transitionDone();
}

void IgnitionSystem::shutdown() {
    transitionDone();
}

}  // namespace body_ecu::adapters
