#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace body_ecu::ports {

using SignalValue =
    std::variant<bool, int32_t, float, std::string, std::vector<uint8_t>>;

using SignalCallback = std::function<void(const std::string&, const SignalValue&)>;

class ISignalBus {
public:
    virtual ~ISignalBus() = default;

    virtual bool publish(const std::string& path,
                         const SignalValue& value) = 0;
    virtual void subscribe(const std::string& path,
                           SignalCallback callback) = 0;
    virtual std::optional<SignalValue> get(
        const std::string& path) const = 0;
};

}  // namespace body_ecu::ports
