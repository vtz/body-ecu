#include "DoorLockSystem.h"

namespace body_ecu::adapters {

DoorLockSystem::DoorLockSystem(ports::IGpioPort& gpio,
                               ports::IButtonInput& button,
                               SomeIpSystem& someip,
                               const body::DoorLockConfig& config,
                               ports::ISignalBus* signal_bus)
    : controller_(gpio, button, someip, config, signal_bus) {}

void DoorLockSystem::init() {
    controller_.init();
}

void DoorLockSystem::run() {
    // Controller is event-driven via SOME/IP method handlers
    // and button interrupt callback.
}

void DoorLockSystem::shutdown() {
    controller_.unlock();
}

}  // namespace body_ecu::adapters
