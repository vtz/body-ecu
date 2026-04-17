#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace body_ecu::ports {

using CloudMessageCallback =
    std::function<void(const std::string&, const std::vector<uint8_t>&)>;

class ICloudTransport {
public:
    virtual ~ICloudTransport() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool publish(const std::string& subject,
                         const std::vector<uint8_t>& data) = 0;
    virtual void subscribe(const std::string& subject,
                           CloudMessageCallback callback) = 0;
};

}  // namespace body_ecu::ports
