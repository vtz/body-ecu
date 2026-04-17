#pragma once

#include "SomeIpSystem.h"
#include <lifecycle/SimpleLifecycleComponent.h>
#include "lighting/LightingController.h"
#include "ports/IGpioPort.h"

namespace body_ecu::adapters {

class LightingSystem : public lifecycle::SimpleLifecycleComponent {
public:
    LightingSystem(ports::IGpioPort& gpio, SomeIpSystem& someip,
                   const body::LightingConfig& config = {});

    void init() override;
    void run() override;
    void shutdown() override;

    body::LightingController& controller() { return controller_; }
    const body::LightingController& controller() const { return controller_; }

private:
    body::LightingController controller_;
};

}  // namespace body_ecu::adapters
