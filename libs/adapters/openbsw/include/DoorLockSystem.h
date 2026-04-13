#pragma once

#include "SomeIpSystem.h"
#include "door_lock/DoorLockController.h"
#include "ports/IButtonInput.h"
#include "ports/IGpioPort.h"
#include "ports/ISignalBus.h"

namespace body_ecu::adapters {

/// AsyncLifecycleComponent wrapper that owns a DoorLockController.
class DoorLockSystem {
public:
    DoorLockSystem(ports::IGpioPort& gpio, ports::IButtonInput& button,
                   SomeIpSystem& someip,
                   const body::DoorLockConfig& config = {},
                   ports::ISignalBus* signal_bus = nullptr);

    void init();
    void run();
    void shutdown();

    body::DoorLockController& controller() { return controller_; }
    const body::DoorLockController& controller() const { return controller_; }

private:
    body::DoorLockController controller_;
};

}  // namespace body_ecu::adapters
