#pragma once

#include <string>

#include "ports/ICloudTransport.h"

namespace body_ecu::adapters {

struct NatsConfig {
    std::string url{"nats://localhost:4222"};
};

/// ICloudTransport implementation backed by NATS via nats.c client.
/// Requires libnats at link time.
class NatsCloudTransportAdapter : public ports::ICloudTransport {
public:
    explicit NatsCloudTransportAdapter(const NatsConfig& config = {});

    bool connect() override;
    void disconnect() override;
    bool publish(const std::string& subject,
                 const std::vector<uint8_t>& data) override;
    void subscribe(const std::string& subject,
                   ports::CloudMessageCallback callback) override;

private:
    NatsConfig config_;
    bool connected_{false};
    // natsConnection* and natsSubscription* would be members here once
    // nats.c is wired into the build.
};

}  // namespace body_ecu::adapters
