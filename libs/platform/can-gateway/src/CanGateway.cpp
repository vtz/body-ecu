#include "can_gateway/CanGateway.h"

#include "can_gateway/MessageTranslator.h"

namespace body_ecu::platform {

CanGateway::CanGateway(ports::ICanBus& can, ports::ISomeIpService& someip)
    : can_(can), someip_(someip) {}

void CanGateway::addMapping(const ServiceMapping& mapping) {
    mappings_.push_back(mapping);
}

void CanGateway::start() {
    buildIndices();

    for (const auto& m : mappings_) {
        if (m.direction == GatewayDirection::SomeIpToCan) {
            someip_.registerMethod(
                m.someip_service_id, m.someip_method_id,
                [this](const ports::SomeIpMessage& msg) {
                    onSomeIpMessage(msg);
                    ports::SomeIpMessage resp = msg;
                    resp.message_type = 0x80;
                    resp.return_code = 0x00;
                    return resp;
                });
        }
        if (m.direction == GatewayDirection::CanToSomeIp) {
            someip_.registerEvent(m.someip_service_id, m.someip_event_id,
                                  m.someip_eventgroup_id);
        }
    }

    if (!rx_registered_) {
        can_.addRxCallback([this](const ports::CanFrame& frame) {
            onCanFrame(frame);
        });
        rx_registered_ = true;
    }

    running_ = true;
}

void CanGateway::stop() {
    running_ = false;
}

void CanGateway::onSomeIpMessage(const ports::SomeIpMessage& msg) {
    auto key = someipKey(msg.service_id, msg.method_id);
    auto it = someip_to_can_index_.find(key);
    if (it == someip_to_can_index_.end()) return;

    auto frame = MessageTranslator::someipToCanFrame(
        it->second->can_id, it->second->can_dlc, msg.payload);
    can_.send(frame);
}

void CanGateway::onCanFrame(const ports::CanFrame& frame) {
    auto it = can_to_someip_index_.find(frame.id);
    if (it == can_to_someip_index_.end()) return;

    auto payload = MessageTranslator::canFrameToSomeipPayload(frame);
    someip_.sendEvent(it->second->someip_service_id,
                      it->second->someip_event_id, payload);
}

void CanGateway::buildIndices() {
    someip_to_can_index_.clear();
    can_to_someip_index_.clear();
    for (const auto& m : mappings_) {
        if (m.direction == GatewayDirection::SomeIpToCan) {
            someip_to_can_index_[someipKey(m.someip_service_id,
                                           m.someip_method_id)] = &m;
        } else {
            can_to_someip_index_[m.can_id] = &m;
        }
    }
}

}  // namespace body_ecu::platform
