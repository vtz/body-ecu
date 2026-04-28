#pragma once

#include <cstdint>

#include "ports/IButtonInput.h"
#include "ports/IDiagDataProvider.h"
#include "ports/IGpioPort.h"
#include "ports/IModeObserver.h"
#include "ports/ISignalBus.h"
#include "ports/ISomeIpService.h"
#include "someip_service_ids.h"

namespace body_ecu::body {

enum class LockState : uint8_t {
    Unlocked = 0,
    Locked = 1,
    Error = 2,
};

struct DoorLockConfig {
    uint16_t service_id{someip::door_lock::kServiceId};
    uint16_t lock_method{someip::door_lock::method::kLock};
    uint16_t unlock_method{someip::door_lock::method::kUnlock};
    uint16_t get_status_method{someip::door_lock::method::kGetStatus};
    uint16_t lock_state_changed_event{someip::door_lock::event::kLockStateChanged};
    uint16_t eventgroup_id{someip::door_lock::eventgroup::kDoorEvents};
    uint32_t lock_gpio_pin{10};
    uint16_t diag_did{0xF101};

    std::string signal_is_locked{
        "Vehicle.Cabin.Door.Row1.DriverSide.IsLocked"};
    std::string signal_command_lock{"Vehicle.Command.Door.Lock"};
    std::string signal_command_response{"Vehicle.Command.Door.Response"};
    std::string signal_speed{"Vehicle.Speed"};
    std::string signal_door_open{
        "Vehicle.Cabin.Door.Row1.DriverSide.IsOpen"};
};

class DoorLockController : public ports::IModeObserver,
                           public ports::IDiagDataProvider {
public:
    DoorLockController(ports::IGpioPort& gpio, ports::IButtonInput& button,
                       ports::ISomeIpService& someip,
                       const DoorLockConfig& config = {},
                       ports::ISignalBus* signal_bus = nullptr);

    void init();

    bool lock();
    bool unlock();
    LockState getState() const { return state_; }

    void setError();

    // IModeObserver
    void onModeChanged(ports::VehicleMode old_mode,
                       ports::VehicleMode new_mode) override;

    // IDiagDataProvider
    std::optional<ports::DiagData> readData(uint16_t did) const override;
    bool ioControl(uint16_t did,
                   const std::vector<uint8_t>& control_param) override;

private:
    ports::SomeIpMessage handleLock(const ports::SomeIpMessage& request);
    ports::SomeIpMessage handleUnlock(const ports::SomeIpMessage& request);
    ports::SomeIpMessage handleGetStatus(const ports::SomeIpMessage& request);
    void publishStateChanged(LockState old_state, LockState new_state);
    void onButtonPress();
    void onCommandSignal(const std::string& path,
                         const ports::SignalValue& value);
    bool checkSafetyConstraints() const;

    ports::IGpioPort& gpio_;
    ports::IButtonInput& button_;
    ports::ISomeIpService& someip_;
    DoorLockConfig config_;
    ports::ISignalBus* signal_bus_;
    LockState state_{LockState::Unlocked};
};

}  // namespace body_ecu::body
