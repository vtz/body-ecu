#pragma once

#include <string>
#include <vector>

#include "ports/ICloudTransport.h"
#include "ports/ISignalBus.h"

namespace body_ecu::platform {

struct CloudGatewayConfig {
    std::string vin{"WVWZZZ3CZWE000001"};
    std::string subject_command_lock{"vehicles.{vin}.command.door.lock"};
    std::string subject_command_response{
        "vehicles.{vin}.command.door.response"};
    std::string subject_state_locked{"vehicles.{vin}.state.door.locked"};
    std::string signal_command_lock{"Vehicle.Command.Door.Lock"};
    std::string signal_command_response{"Vehicle.Command.Door.Response"};
    std::string signal_is_locked{
        "Vehicle.Cabin.Door.Row1.DriverSide.IsLocked"};
};

class CloudGatewayClient {
public:
    CloudGatewayClient(ports::ICloudTransport& transport,
                       ports::ISignalBus& signal_bus,
                       const CloudGatewayConfig& config = {});

    void init();
    void shutdown();

    bool isConnected() const { return connected_; }

private:
    std::string resolveSubject(const std::string& pattern) const;
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
