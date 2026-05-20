#include "vehicle_mode/VehicleModeManager.h"

namespace body_ecu::body {

VehicleModeManager::VehicleModeManager(ports::ISomeIpService& someip,
                                       const VehicleModeConfig& config)
    : someip_(someip), config_(config) {}

void VehicleModeManager::init() {
    someip_.registerMethod(config_.service_id, config_.getter_method,
                           [this](const ports::SomeIpMessage& req) {
                               return handleGet(req);
                           });
    someip_.registerMethod(config_.service_id, config_.setter_method,
                           [this](const ports::SomeIpMessage& req) {
                               return handleSet(req);
                           });
    someip_.registerEvent(config_.service_id, config_.notifier_event,
                          config_.eventgroup_id);
}

bool VehicleModeManager::setMode(ports::VehicleMode mode) {
    if (!isValidTransition(mode_, mode)) return false;
    if (mode == mode_) return true;

    auto old = mode_;
    mode_ = mode;
    publishModeChanged();
    notifyObservers(old, mode);
    return true;
}

void VehicleModeManager::addObserver(ports::IModeObserver* observer) {
    observers_.push_back(observer);
}

bool VehicleModeManager::isValidTransition(ports::VehicleMode from,
                                           ports::VehicleMode to) const {
    if (from == to) return true;

    using M = ports::VehicleMode;
    switch (from) {
        case M::Off:
            return to == M::Accessory;
        case M::Accessory:
            return to == M::Off || to == M::Run || to == M::Crank;
        case M::Run:
            return to == M::Accessory || to == M::Crank;
        case M::Crank:
            return to == M::Run || to == M::Accessory;
    }
    return false;
}

ports::SomeIpMessage VehicleModeManager::handleGet(
    const ports::SomeIpMessage& request) {
    ports::SomeIpMessage response = request;
    response.message_type = 0x80;
    response.return_code = 0x00;
    response.payload = {static_cast<uint8_t>(mode_)};
    return response;
}

ports::SomeIpMessage VehicleModeManager::handleSet(
    const ports::SomeIpMessage& request) {
    ports::SomeIpMessage response = request;
    response.message_type = 0x80;

    if (request.payload.empty()) {
        response.return_code = 0x01;
        return response;
    }

    auto target = static_cast<ports::VehicleMode>(request.payload[0]);
    response.return_code = setMode(target) ? 0x00 : 0x01;
    return response;
}

void VehicleModeManager::publishModeChanged() {
    someip_.sendEvent(config_.service_id, config_.notifier_event,
                      {static_cast<uint8_t>(mode_)});
}

void VehicleModeManager::notifyObservers(ports::VehicleMode old_mode,
                                         ports::VehicleMode new_mode) {
    for (auto* obs : observers_) {
        obs->onModeChanged(old_mode, new_mode);
    }
}

}  // namespace body_ecu::body
