#include "SpeedSimulatorSystem.h"

namespace body_ecu::adapters {

SpeedSimulatorSystem::SpeedSimulatorSystem(
    ports::IAdcInput& adc, SomeIpSystem& someip,
    ports::ITimerService& timer,
    const body::SpeedSimulatorConfig& config,
    ports::ISignalBus* signal_bus)
    : simulator_(adc, someip, timer, config, signal_bus) {}

void SpeedSimulatorSystem::init() {
    simulator_.init();
    transitionDone();
}

void SpeedSimulatorSystem::run() {
    transitionDone();
}

void SpeedSimulatorSystem::shutdown() {
    transitionDone();
}

}  // namespace body_ecu::adapters
