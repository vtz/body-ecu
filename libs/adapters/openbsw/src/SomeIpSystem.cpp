#include "SomeIpSystem.h"

namespace body_ecu::adapters {

SomeIpSystem::SomeIpSystem(const SomeIpConfig& config) : config_(config) {}

void SomeIpSystem::init() {
    state_ = LifecycleState::Initialized;
}

void SomeIpSystem::run() {
    state_ = LifecycleState::Running;
    // In the full build, this starts the OpenSOME/IP UDP transport
    // listener loop on an async context.
}

void SomeIpSystem::shutdown() {
    state_ = LifecycleState::Shutdown;
    methods_.clear();
    events_.clear();
}

void SomeIpSystem::registerMethod(uint16_t service_id, uint16_t method_id,
                                  ports::MethodHandler handler) {
    methods_[makeKey(service_id, method_id)] = std::move(handler);
}

void SomeIpSystem::registerEvent(uint16_t service_id, uint16_t event_id,
                                 uint16_t eventgroup_id) {
    events_.push_back({service_id, event_id, eventgroup_id});
}

void SomeIpSystem::sendEvent(uint16_t service_id, uint16_t event_id,
                             const std::vector<uint8_t>& payload) {
    ports::SomeIpMessage msg;
    msg.service_id = service_id;
    msg.method_id = event_id;
    msg.message_type = 0x02;  // Notification
    msg.return_code = 0x00;
    msg.payload = payload;
    sent_events_.push_back(msg);
    // In the full build, this serializes and sends over the transport.
}

void SomeIpSystem::sendResponse(const ports::SomeIpMessage& response) {
    sent_responses_.push_back(response);
    // In the full build, this serializes and sends over the transport.
}

ports::SomeIpMessage SomeIpSystem::dispatch(
    const ports::SomeIpMessage& request) {
    auto it = methods_.find(makeKey(request.service_id, request.method_id));
    if (it == methods_.end()) {
        ports::SomeIpMessage err = request;
        err.message_type = 0x81;  // Error
        err.return_code = 0x05;   // E_NOT_OK
        return err;
    }
    return it->second(request);
}

}  // namespace body_ecu::adapters
