#pragma once

#include <cstdint>
#include <functional>

#include "ports/IButtonInput.h"
#include "ports/ISignalBus.h"
#include "ports/ITimerService.h"
#include "vehicle_mode/VehicleModeManager.h"

namespace body_ecu::body {

struct IgnitionConfig {
    uint32_t crank_duration_ms{1000};
    std::string signal_speed{"Vehicle.Speed"};
};

class IgnitionController {
public:
    IgnitionController(ports::IButtonInput& button,
                       VehicleModeManager& mode_manager,
                       ports::ITimerService& timer,
                       const IgnitionConfig& config = {},
                       ports::ISignalBus* signal_bus = nullptr);

    void init();

private:
    void onButtonPress();
    float getCurrentSpeed() const;

    ports::IButtonInput& button_;
    VehicleModeManager& mode_manager_;
    ports::ITimerService& timer_;
    IgnitionConfig config_;
    ports::ISignalBus* signal_bus_;
    bool cranking_{false};
};

}  // namespace body_ecu::body
