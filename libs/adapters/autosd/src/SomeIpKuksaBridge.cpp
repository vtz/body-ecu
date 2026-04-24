#include "autosd_adapters/SomeIpKuksaBridge.h"

#include <cstring>

namespace body_ecu::adapters {

SomeIpKuksaBridge::SomeIpKuksaBridge(ports::ISomeIpService& someip,
                                     ports::ISignalBus& signal_bus)
    : someip_(someip), signal_bus_(signal_bus) {}

void SomeIpKuksaBridge::addMapping(const BridgeMapping& mapping) {
    mappings_.push_back(mapping);
}

void SomeIpKuksaBridge::init() {
    for (const auto& m : mappings_) {
        if (m.direction == BridgeDirection::EventToSignal) {
            someip_.registerMethod(
                m.someip_service_id, m.someip_method_or_event_id,
                [this](const ports::SomeIpMessage& msg) {
                    onSomeIpEvent(msg);
                    ports::SomeIpMessage ack = msg;
                    ack.message_type = 0x80;
                    ack.return_code = 0x00;
                    return ack;
                });
        } else {
            signal_bus_.subscribe(
                m.signal_path,
                [this, m](const std::string& path,
                          const ports::SignalValue& value) {
                    onSignalChanged(m, path, value);
                });
        }
    }
}

void SomeIpKuksaBridge::shutdown() {}

void SomeIpKuksaBridge::onSomeIpEvent(const ports::SomeIpMessage& msg) {
    for (const auto& m : mappings_) {
        if (m.direction == BridgeDirection::EventToSignal &&
            m.someip_service_id == msg.service_id &&
            m.someip_method_or_event_id == msg.method_id) {
            if (msg.payload.size() == 4) {
                uint32_t bits = (static_cast<uint32_t>(msg.payload[0]) << 24) |
                                (static_cast<uint32_t>(msg.payload[1]) << 16) |
                                (static_cast<uint32_t>(msg.payload[2]) << 8) |
                                 static_cast<uint32_t>(msg.payload[3]);
                float value;
                std::memcpy(&value, &bits, sizeof(value));
                signal_bus_.publish(m.signal_path,
                                   ports::SignalValue{value});
            } else if (!msg.payload.empty()) {
                int32_t value = 0;
                for (auto byte : msg.payload) {
                    value = (value << 8) | byte;
                }
                signal_bus_.publish(m.signal_path,
                                   ports::SignalValue{value});
            }
            return;
        }
    }
}

void SomeIpKuksaBridge::onSignalChanged(
    const BridgeMapping& mapping, const std::string& /*path*/,
    const ports::SignalValue& value) {
    ports::SomeIpMessage request;
    request.service_id = mapping.someip_service_id;
    request.method_id = mapping.someip_method_or_event_id;
    request.message_type = 0x00;

    auto* b = std::get_if<bool>(&value);
    if (b) {
        request.payload = {static_cast<uint8_t>(*b ? 1 : 0)};
    }

    someip_.sendResponse(request);
}

}  // namespace body_ecu::adapters
