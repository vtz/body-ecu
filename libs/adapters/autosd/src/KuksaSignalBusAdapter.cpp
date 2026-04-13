#include "autosd_adapters/KuksaSignalBusAdapter.h"

#include <cstdio>

namespace body_ecu::adapters {

KuksaSignalBusAdapter::KuksaSignalBusAdapter(const KuksaConfig& config)
    : config_(config) {}

void KuksaSignalBusAdapter::connect() {
    std::printf("[Kuksa] Connecting to %s:%u\n", config_.host.c_str(),
                config_.port);
    // TODO: create gRPC channel to kuksa-databroker
    connected_ = true;
}

void KuksaSignalBusAdapter::disconnect() {
    connected_ = false;
}

bool KuksaSignalBusAdapter::publish(const std::string& path,
                                    const ports::SignalValue& value) {
    if (!connected_) return false;
    std::printf("[Kuksa] SET %s\n", path.c_str());
    // TODO: call kuksa val.proto SetDatapoints
    (void)value;
    return true;
}

void KuksaSignalBusAdapter::subscribe(const std::string& path,
                                      ports::SignalCallback callback) {
    if (!connected_) return;
    std::printf("[Kuksa] SUBSCRIBE %s\n", path.c_str());
    // TODO: call kuksa val.proto Subscribe, dispatch callback on updates
    (void)callback;
}

std::optional<ports::SignalValue> KuksaSignalBusAdapter::get(
    const std::string& path) const {
    if (!connected_) return std::nullopt;
    std::printf("[Kuksa] GET %s\n", path.c_str());
    // TODO: call kuksa val.proto GetDatapoints
    return std::nullopt;
}

}  // namespace body_ecu::adapters
