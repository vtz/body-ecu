#pragma once

#include <cstdint>
#include <vector>

#include "ports/IModeObserver.h"
#include "ports/ISomeIpService.h"
#include "someip_service_ids.h"

namespace body_ecu::body {

struct VehicleModeConfig {
    uint16_t service_id{someip::vehicle_mode::kServiceId};
    uint16_t getter_method{someip::vehicle_mode::field::kModeGetter};
    uint16_t setter_method{someip::vehicle_mode::field::kModeSetter};
    uint16_t notifier_event{someip::vehicle_mode::field::kModeNotifier};
    uint16_t eventgroup_id{someip::vehicle_mode::eventgroup::kModeEvents};
};

/// Thread-safety contract: addObserver() must only be called during
/// initialization (before run). setMode()/getMode() are single-threaded
/// (main loop or timer callback context). Do not call from multiple threads
/// concurrently.
class VehicleModeManager {
public:
    VehicleModeManager(ports::ISomeIpService& someip,
                       const VehicleModeConfig& config = {});

    void init();

    ports::VehicleMode getMode() const { return mode_; }
    bool setMode(ports::VehicleMode mode);

    /// Register an observer. Must be called before run() to avoid races.
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
