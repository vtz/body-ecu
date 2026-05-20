#include "autosd_adapters/SomeIpKuksaBridge.h"

#include <cstdio>
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
            someip_.registerMethod(
                m.someip_service_id, m.someip_method_or_event_id,
                [](const ports::SomeIpMessage& msg) {
                    std::printf("[Bridge] Response svc=0x%04X method=0x%04X rc=0x%02X\n",
                                msg.service_id, msg.method_id, msg.return_code);
                    return msg;
                });
            if (m.someip_false_method_id != 0) {
                someip_.registerMethod(
                    m.someip_service_id, m.someip_false_method_id,
                    [](const ports::SomeIpMessage& msg) {
                        std::printf("[Bridge] Response svc=0x%04X method=0x%04X rc=0x%02X\n",
                                    msg.service_id, msg.method_id, msg.return_code);
                        return msg;
                    });
            }
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
            if (msg.payload.empty()) return;

            switch (m.datatype) {
            case SignalDataType::Bool: {
                bool val = msg.payload.back() != 0;
                signal_bus_.publish(m.signal_path, ports::SignalValue{val});
                break;
            }
            case SignalDataType::Float: {
                if (msg.payload.size() >= 4) {
                    uint32_t bits = (static_cast<uint32_t>(msg.payload[0]) << 24) |
                                    (static_cast<uint32_t>(msg.payload[1]) << 16) |
                                    (static_cast<uint32_t>(msg.payload[2]) << 8) |
                                     static_cast<uint32_t>(msg.payload[3]);
                    float value;
                    std::memcpy(&value, &bits, sizeof(value));
                    signal_bus_.publish(m.signal_path, ports::SignalValue{value});
                }
                break;
            }
            case SignalDataType::Bitmask: {
                int32_t bitmask = 0;
                for (size_t i = 0; i < msg.payload.size(); ++i) {
                    if (msg.payload[i] != 0) bitmask |= (1 << i);
                }
                signal_bus_.publish(m.signal_path, ports::SignalValue{bitmask});
                break;
            }
            case SignalDataType::String: {
                std::string s(msg.payload.begin(), msg.payload.end());
                auto nul = s.find('\0');
                if (nul != std::string::npos) s.erase(nul);
                signal_bus_.publish(m.signal_path, ports::SignalValue{std::move(s)});
                break;
            }
            case SignalDataType::Int32:
            default: {
                int32_t value = 0;
                for (auto byte : msg.payload) {
                    value = (value << 8) | byte;
                }
                signal_bus_.publish(m.signal_path, ports::SignalValue{value});
                break;
            }
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

    if (mapping.datatype == SignalDataType::Packed2Bytes) {
        auto* v = std::get_if<int32_t>(&value);
        if (v) {
            request.payload = {
                static_cast<uint8_t>((*v >> 8) & 0xFF),
                static_cast<uint8_t>(*v & 0xFF)};
        }
    } else {
        auto* b = std::get_if<bool>(&value);
        if (b) {
            if (!*b && mapping.someip_false_method_id != 0) {
                request.method_id = mapping.someip_false_method_id;
            }
            request.payload = {static_cast<uint8_t>(*b ? 1 : 0)};
        }
    }

    std::printf("[Bridge] CMD svc=0x%04X method=0x%04X payload=[",
                request.service_id, request.method_id);
    for (size_t i = 0; i < request.payload.size(); ++i) {
        if (i > 0) std::printf(" ");
        std::printf("0x%02X", request.payload[i]);
    }
    std::printf("]\n");

    someip_.sendResponse(request);
}

}  // namespace body_ecu::adapters
