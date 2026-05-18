#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ports/ICloudTransport.h"

namespace body_ecu::adapters {

struct NatsConfig {
    std::string url{"nats://localhost:4222"};
};

/// ICloudTransport implementation backed by NATS via nats.c client.
///
/// When built with HAS_NATS, uses real nats.c calls.
/// Otherwise falls back to printf stubs for development builds.
class NatsCloudTransportAdapter : public ports::ICloudTransport {
public:
    explicit NatsCloudTransportAdapter(const NatsConfig& config = {});
    ~NatsCloudTransportAdapter();

    NatsCloudTransportAdapter(const NatsCloudTransportAdapter&) = delete;
    NatsCloudTransportAdapter& operator=(const NatsCloudTransportAdapter&) = delete;

    bool connect() override;
    void disconnect() override;
    bool publish(const std::string& subject,
                 const std::vector<uint8_t>& data) override;
    void subscribe(const std::string& subject,
                   ports::CloudMessageCallback callback) override;

    /// Internal dispatch for the nats.c C-style callback. Not part of public API.
    void dispatchMessage(const std::string& subject,
                         const std::vector<uint8_t>& data);

    /// Returns true if a NATS subject matches a pattern containing '*'
    /// (single-token) and '>' (tail-match) wildcards.
    static bool subjectMatchesPattern(const std::string& pattern,
                                      const std::string& subject);

private:
    NatsConfig config_;
    bool connected_{false};

    std::mutex mutex_;
    std::unordered_map<std::string, ports::CloudMessageCallback> subscribers_;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace body_ecu::adapters
