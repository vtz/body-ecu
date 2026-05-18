#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace body_ecu::ports {

struct CanFrame {
    uint32_t id{0};
    uint8_t dlc{0};
    uint8_t data[64]{};
};

static constexpr uint8_t kCanMaxClassicDlc = 8;
static constexpr uint8_t kCanMaxFdDlc = 64;

using CanRxCallback = std::function<void(const CanFrame&)>;

class ICanBus {
public:
    virtual ~ICanBus() = default;
    virtual bool send(const CanFrame& frame) = 0;

    /// Register an additional RX listener. Multiple listeners are supported;
    /// each receives every incoming frame.
    virtual void addRxCallback(CanRxCallback callback) = 0;

    /// @deprecated Use addRxCallback(). Kept for source compatibility;
    /// default implementation forwards to addRxCallback().
    virtual void setRxCallback(CanRxCallback callback) {
        addRxCallback(std::move(callback));
    }
};

}  // namespace body_ecu::ports
