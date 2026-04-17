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
    transitionDone();
}

void DoorLockSystem::run() {
    transitionDone();
}

void DoorLockSystem::shutdown() {
    controller_.unlock();
    transitionDone();
}

}  // namespace body_ecu::adapters
