#pragma once

#include <string>

#include "ports/ISignalBus.h"

namespace body_ecu::adapters {

struct KuksaConfig {
    std::string host{"localhost"};
    uint16_t port{55555};
};

/// ISignalBus implementation backed by Eclipse Kuksa Databroker via gRPC.
/// Requires grpc++ and kuksa val.proto stubs at link time.
class KuksaSignalBusAdapter : public ports::ISignalBus {
public:
    explicit KuksaSignalBusAdapter(const KuksaConfig& config = {});

    void connect();
    void disconnect();

    bool publish(const std::string& path,
                 const ports::SignalValue& value) override;
    void subscribe(const std::string& path,
                   ports::SignalCallback callback) override;
    std::optional<ports::SignalValue> get(
        const std::string& path) const override;

private:
    KuksaConfig config_;
    bool connected_{false};
    // gRPC channel and stubs would be members here once
    // kuksa-databroker-proto is wired into the build.
};

}  // namespace body_ecu::adapters
