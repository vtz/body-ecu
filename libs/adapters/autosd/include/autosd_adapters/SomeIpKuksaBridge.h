#pragma once

#include <string>
#include <vector>

#include "ports/ISignalBus.h"
#include "ports/ISomeIpService.h"

namespace body_ecu::adapters {

enum class BridgeDirection { EventToSignal, SignalToMethod };
enum class SignalDataType { Int32, Bool, Float, String, Bitmask, Packed2Bytes };

struct BridgeMapping {
    std::string signal_path;
    BridgeDirection direction{BridgeDirection::EventToSignal};
    SignalDataType datatype{SignalDataType::Int32};
    uint16_t someip_service_id{0};
    uint16_t someip_method_or_event_id{0};
    uint16_t someip_eventgroup_id{0};
    uint16_t someip_false_method_id{0};
};

/// Bridges SOME/IP events/methods on the MCU with VSS signals in Kuksa
/// on the MPU. Runs as a SOME/IP client: subscribes to MCU events and
/// writes to ISignalBus, watches ISignalBus for commands and sends
/// SOME/IP method requests.
class SomeIpKuksaBridge {
public:
    SomeIpKuksaBridge(ports::ISomeIpService& someip,
                      ports::ISignalBus& signal_bus);

    void addMapping(const BridgeMapping& mapping);
    void init();
    void shutdown();

    const std::vector<BridgeMapping>& mappings() const { return mappings_; }

private:
    void onSomeIpEvent(const ports::SomeIpMessage& msg);
    void onSignalChanged(const BridgeMapping& mapping,
                         const std::string& path,
                         const ports::SignalValue& value);

    ports::ISomeIpService& someip_;
    ports::ISignalBus& signal_bus_;
    std::vector<BridgeMapping> mappings_;
};

}  // namespace body_ecu::adapters
