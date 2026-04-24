#pragma once

#include "SomeIpSystem.h"
#include "VehicleModeSystem.h"
#include <lifecycle/SimpleLifecycleComponent.h>
#include "ignition/IgnitionController.h"
#include "ports/IButtonInput.h"
#include "ports/ISignalBus.h"
#include "ports/ITimerService.h"

namespace body_ecu::adapters {

class IgnitionSystem : public lifecycle::SimpleLifecycleComponent {
public:
    IgnitionSystem(ports::IButtonInput& button,
                   VehicleModeSystem& vehicle_mode,
                   ports::ITimerService& timer,
                   const body::IgnitionConfig& config = {},
                   ports::ISignalBus* signal_bus = nullptr);

    void init() override;
    void run() override;
    void shutdown() override;

private:
    body::IgnitionController controller_;
};

}  // namespace body_ecu::adapters
