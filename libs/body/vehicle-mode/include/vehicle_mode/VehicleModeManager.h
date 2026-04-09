#pragma once

#include <cstdint>
#include <vector>

#include "ports/IModeObserver.h"
#include "ports/ISomeIpService.h"

namespace body_ecu::body {

struct VehicleModeConfig {
    uint16_t service_id{0x1002};
    uint16_t getter_method{0x0001};
    uint16_t setter_method{0x0002};
    uint16_t notifier_event{0x8001};
    uint16_t eventgroup_id{0x0001};
};

class VehicleModeManager {
public:
    VehicleModeManager(ports::ISomeIpService& someip,
                       const VehicleModeConfig& config = {});

    void init();

    ports::VehicleMode getMode() const { return mode_; }
    bool setMode(ports::VehicleMode mode);

    void addObserver(ports::IModeObserver* observer);

private:
    ports::SomeIpMessage handleGet(const ports::SomeIpMessage& request);
    ports::SomeIpMessage handleSet(const ports::SomeIpMessage& request);
    void publishModeChanged();
    void notifyObservers(ports::VehicleMode old_mode,
                         ports::VehicleMode new_mode);
    bool isValidTransition(ports::VehicleMode from,
                           ports::VehicleMode to) const;

    ports::ISomeIpService& someip_;
    VehicleModeConfig config_;
    ports::VehicleMode mode_{ports::VehicleMode::Off};
    std::vector<ports::IModeObserver*> observers_;
};

}  // namespace body_ecu::body
