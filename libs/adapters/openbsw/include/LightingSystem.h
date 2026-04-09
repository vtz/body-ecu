#pragma once

#include "SomeIpSystem.h"
#include "lighting/LightingController.h"
#include "ports/IGpioPort.h"

namespace body_ecu::adapters {

/// AsyncLifecycleComponent wrapper that owns a LightingController
/// and injects platform adapters. Manages lifecycle for the OpenBSW
/// LifecycleManager.
class LightingSystem {
public:
    LightingSystem(ports::IGpioPort& gpio, SomeIpSystem& someip,
                   const body::LightingConfig& config = {});

    void init();
    void run();
    void shutdown();

    body::LightingController& controller() { return controller_; }
    const body::LightingController& controller() const { return controller_; }

private:
    body::LightingController controller_;
};

}  // namespace body_ecu::adapters
