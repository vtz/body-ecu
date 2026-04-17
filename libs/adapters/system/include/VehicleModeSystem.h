#pragma once

#include "SomeIpSystem.h"
#include <lifecycle/SimpleLifecycleComponent.h>
#include "vehicle_mode/VehicleModeManager.h"

namespace body_ecu::adapters {

class VehicleModeSystem : public lifecycle::SimpleLifecycleComponent {
public:
    VehicleModeSystem(SomeIpSystem& someip,
                      const body::VehicleModeConfig& config = {});

    void init() override;
    void run() override;
    void shutdown() override;

    body::VehicleModeManager& manager() { return manager_; }
    const body::VehicleModeManager& manager() const { return manager_; }

private:
    body::VehicleModeManager manager_;
};

}  // namespace body_ecu::adapters
