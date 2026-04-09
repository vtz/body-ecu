#pragma once

#include "SomeIpSystem.h"
#include "vehicle_mode/VehicleModeManager.h"

namespace body_ecu::adapters {

/// AsyncLifecycleComponent wrapper that owns a VehicleModeManager.
class VehicleModeSystem {
public:
    VehicleModeSystem(SomeIpSystem& someip,
                      const body::VehicleModeConfig& config = {});

    void init();
    void run();
    void shutdown();

    body::VehicleModeManager& manager() { return manager_; }
    const body::VehicleModeManager& manager() const { return manager_; }

private:
    body::VehicleModeManager manager_;
};

}  // namespace body_ecu::adapters
