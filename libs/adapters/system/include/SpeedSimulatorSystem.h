#pragma once

#include <lifecycle/SimpleLifecycleComponent.h>

#include "SomeIpSystem.h"
#include "ports/IAdcInput.h"
#include "ports/ISignalBus.h"
#include "ports/ITimerService.h"
#include "speed_simulator/SpeedSimulator.h"

namespace body_ecu::adapters {

class SpeedSimulatorSystem : public lifecycle::SimpleLifecycleComponent {
public:
    SpeedSimulatorSystem(ports::IAdcInput& adc, SomeIpSystem& someip,
                         ports::ITimerService& timer,
                         const body::SpeedSimulatorConfig& config = {},
                         ports::ISignalBus* signal_bus = nullptr);

    void init() override;
    void run() override;
    void shutdown() override;

    body::SpeedSimulator& simulator() { return simulator_; }
    const body::SpeedSimulator& simulator() const { return simulator_; }

private:
    body::SpeedSimulator simulator_;
};

}  // namespace body_ecu::adapters
