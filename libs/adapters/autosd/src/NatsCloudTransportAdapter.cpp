#include "autosd_adapters/NatsCloudTransportAdapter.h"

#include <cstdio>

namespace body_ecu::adapters {

NatsCloudTransportAdapter::NatsCloudTransportAdapter(const NatsConfig& config)
    : config_(config) {}

bool NatsCloudTransportAdapter::connect() {
    std::printf("[NATS] Connecting to %s\n", config_.url.c_str());
    // TODO: natsConnection_ConnectTo(...)
    connected_ = true;
    return true;
}

void NatsCloudTransportAdapter::disconnect() {
    // TODO: natsConnection_Close(...)
    connected_ = false;
}

bool NatsCloudTransportAdapter::publish(const std::string& subject,
                                        const std::vector<uint8_t>& data) {
    if (!connected_) return false;
    std::printf("[NATS] PUBLISH %s (%zu bytes)\n", subject.c_str(),
                data.size());
    // TODO: natsConnection_Publish(...)
    return true;
}

void NatsCloudTransportAdapter::subscribe(
    const std::string& subject, ports::CloudMessageCallback callback) {
    if (!connected_) return;
    std::printf("[NATS] SUBSCRIBE %s\n", subject.c_str());
    // TODO: natsConnection_Subscribe(...) with message callback
    (void)callback;
}

}  // namespace body_ecu::adapters
