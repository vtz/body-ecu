#pragma once

#include <cstdint>
#include <functional>

namespace body_ecu::ports {

struct CanFrame {
    uint32_t id{0};
    uint8_t dlc{0};
    uint8_t data[64]{};
};

using CanRxCallback = std::function<void(const CanFrame&)>;

class ICanBus {
public:
    virtual ~ICanBus() = default;
    virtual bool send(const CanFrame& frame) = 0;
    virtual void setRxCallback(CanRxCallback callback) = 0;
};

}  // namespace body_ecu::ports
