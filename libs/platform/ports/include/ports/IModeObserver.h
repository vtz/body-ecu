#pragma once

#include <cstdint>

namespace body_ecu::ports {

enum class VehicleMode : uint8_t {
    Off = 0,
    Accessory = 1,
    Run = 2,
    Crank = 3,
};

class IModeObserver {
public:
    virtual ~IModeObserver() = default;
    virtual void onModeChanged(VehicleMode old_mode,
                               VehicleMode new_mode) = 0;
};

}  // namespace body_ecu::ports
