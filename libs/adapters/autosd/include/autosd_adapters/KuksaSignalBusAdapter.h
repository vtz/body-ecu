#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ports/ISignalBus.h"

namespace body_ecu::adapters {

struct KuksaConfig {
    std::string host{"localhost"};
    uint16_t port{55555};
};

/// ISignalBus implementation backed by Eclipse Kuksa Databroker via gRPC.
///
/// When built with HAS_KUKSA_GRPC, uses real gRPC calls to the databroker.
/// Otherwise falls back to printf stubs for development builds.
class KuksaSignalBusAdapter : public ports::ISignalBus {
public:
    explicit KuksaSignalBusAdapter(const KuksaConfig& config = {});
    ~KuksaSignalBusAdapter();

    KuksaSignalBusAdapter(const KuksaSignalBusAdapter&) = delete;
    KuksaSignalBusAdapter& operator=(const KuksaSignalBusAdapter&) = delete;

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

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ports::SignalCallback> subscribers_;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace body_ecu::adapters
