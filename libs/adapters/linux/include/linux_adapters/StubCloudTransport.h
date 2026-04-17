#pragma once

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "ports/ICloudTransport.h"

namespace body_ecu::adapters {

class StubCloudTransport : public ports::ICloudTransport {
public:
    bool connect() override;
    void disconnect() override;
    bool publish(const std::string& subject,
                 const std::vector<uint8_t>& data) override;
    void subscribe(const std::string& subject,
                   ports::CloudMessageCallback callback) override;

    /// Inject a message as if it came from the cloud (for testing).
    void injectMessage(const std::string& subject,
                       const std::vector<uint8_t>& data);

    bool isConnected() const { return connected_; }

private:
    bool connected_{false};
    std::map<std::string, std::vector<ports::CloudMessageCallback>>
        subscribers_;
};

}  // namespace body_ecu::adapters
