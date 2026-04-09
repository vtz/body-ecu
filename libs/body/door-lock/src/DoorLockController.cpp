#include "door_lock/DoorLockController.h"

namespace body_ecu::body {

DoorLockController::DoorLockController(ports::IGpioPort& gpio,
                                       ports::IButtonInput& button,
                                       ports::ISomeIpService& someip,
                                       const DoorLockConfig& config)
    : gpio_(gpio), button_(button), someip_(someip), config_(config) {}

void DoorLockController::init() {
    someip_.registerMethod(config_.service_id, config_.lock_method,
                           [this](const ports::SomeIpMessage& req) {
                               return handleLock(req);
                           });
    someip_.registerMethod(config_.service_id, config_.unlock_method,
                           [this](const ports::SomeIpMessage& req) {
                               return handleUnlock(req);
                           });
    someip_.registerMethod(config_.service_id, config_.get_status_method,
                           [this](const ports::SomeIpMessage& req) {
                               return handleGetStatus(req);
                           });
    someip_.registerEvent(config_.service_id,
                          config_.lock_state_changed_event,
                          config_.eventgroup_id);

    button_.onPress([this]() { onButtonPress(); });
}

bool DoorLockController::lock() {
    if (state_ == LockState::Error) return false;
    if (state_ == LockState::Locked) return true;

    LockState old = state_;
    state_ = LockState::Locked;
    gpio_.write(config_.lock_gpio_pin, true);
    publishStateChanged(old, state_);
    return true;
}

bool DoorLockController::unlock() {
    if (state_ == LockState::Error) return false;
    if (state_ == LockState::Unlocked) return true;

    LockState old = state_;
    state_ = LockState::Unlocked;
    gpio_.write(config_.lock_gpio_pin, false);
    publishStateChanged(old, state_);
    return true;
}

void DoorLockController::setError() {
    LockState old = state_;
    state_ = LockState::Error;
    publishStateChanged(old, state_);
}

void DoorLockController::onModeChanged(ports::VehicleMode /*old_mode*/,
                                       ports::VehicleMode new_mode) {
    if (new_mode == ports::VehicleMode::Run) {
        lock();
    }
}

std::optional<ports::DiagData> DoorLockController::readData(
    uint16_t did) const {
    if (did != config_.diag_did) return std::nullopt;
    ports::DiagData data;
    data.did = did;
    data.data.push_back(static_cast<uint8_t>(state_));
    return data;
}

bool DoorLockController::ioControl(
    uint16_t did, const std::vector<uint8_t>& control_param) {
    if (did != config_.diag_did) return false;
    if (control_param.empty()) return false;
    return control_param[0] != 0 ? lock() : unlock();
}

void DoorLockController::onButtonPress() {
    if (state_ == LockState::Locked) {
        unlock();
    } else if (state_ == LockState::Unlocked) {
        lock();
    }
}

ports::SomeIpMessage DoorLockController::handleLock(
    const ports::SomeIpMessage& request) {
    ports::SomeIpMessage response = request;
    response.message_type = 0x80;
    response.return_code = lock() ? 0x00 : 0x01;
    return response;
}

ports::SomeIpMessage DoorLockController::handleUnlock(
    const ports::SomeIpMessage& request) {
    ports::SomeIpMessage response = request;
    response.message_type = 0x80;
    response.return_code = unlock() ? 0x00 : 0x01;
    return response;
}

ports::SomeIpMessage DoorLockController::handleGetStatus(
    const ports::SomeIpMessage& request) {
    ports::SomeIpMessage response = request;
    response.message_type = 0x80;
    response.return_code = 0x00;
    response.payload = {static_cast<uint8_t>(state_)};
    return response;
}

void DoorLockController::publishStateChanged(LockState old_state,
                                             LockState new_state) {
    std::vector<uint8_t> payload = {static_cast<uint8_t>(old_state),
                                    static_cast<uint8_t>(new_state)};
    someip_.sendEvent(config_.service_id, config_.lock_state_changed_event,
                      payload);
}

}  // namespace body_ecu::body
