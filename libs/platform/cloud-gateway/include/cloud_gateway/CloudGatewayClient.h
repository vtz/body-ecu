#pragma once

#include <string>
#include <vector>

#include "ports/ICloudTransport.h"
#include "ports/ISignalBus.h"

namespace body_ecu::platform {

struct CloudGatewayConfig {
    std::string vin{"WVWZZZ3CZWE000001"};

    std::string subject_command_lock{"vehicles.{vin}.command.door.lock"};
    std::string subject_command_response{"vehicles.{vin}.command.door.response"};
    std::string subject_state_locked{"vehicles.{vin}.state.door.locked"};
    std::string subject_state_mode{"vehicles.{vin}.state.vehicle.mode"};
    std::string subject_state_speed{"vehicles.{vin}.state.vehicle.speed"};
    std::string subject_state_lights{"vehicles.{vin}.state.lights.status"};

    std::string signal_command_lock{"Vehicle.Command.Door.Lock"};
    std::string signal_command_response{"Vehicle.Command.Door.Response"};
    std::string signal_is_locked{"Vehicle.Cabin.Door.Row1.DriverSide.IsLocked"};
    std::string subject_command_lights{"vehicles.{vin}.command.lights.set"};

    std::string signal_mode{"Vehicle.Mode"};
    std::string signal_speed{"Vehicle.Speed"};
    std::string signal_lights{"Vehicle.Lights.Status"};
    std::string signal_command_lights{"Vehicle.Command.Lights.Set"};
};

class CloudGatewayClient {
public:
    CloudGatewayClient(ports::ICloudTransport& transport,
                       ports::ISignalBus& signal_bus,
                       const CloudGatewayConfig& config = {});

    void init();
    void shutdown();

    /// Publish current state of all signals to NATS so late-joining
    /// subscribers (e.g. companion app) get the latest values.
    void publishCurrentState();

    /// Update the VIN and re-publish it plus current state to NATS.
    /// Called when the MCU pushes a VIN event after the HPC has started.
    void updateVin(const std::string& vin);

    bool isConnected() const { return connected_; }

private:
    std::string resolveSubject(const std::string& pattern) const;
    std::string resolveSubjectWildcard(const std::string& pattern) const;
    void onCloudCommand(const std::string& subject,
                        const std::vector<uint8_t>& data);
    void onLockStateChanged(const std::string& path,
                            const ports::SignalValue& value);
    void onCommandResponse(const std::string& path,
                           const ports::SignalValue& value);

    ports::ICloudTransport& transport_;
    ports::ISignalBus& signal_bus_;
    CloudGatewayConfig config_;
    bool connected_{false};
};

}  // namespace body_ecu::platform
