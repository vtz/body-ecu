#pragma once

#ifndef BUILD_TESTS

#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>

#include <vector>

#include "ports/ICanBus.h"

namespace body_ecu::adapters {

/// Zephyr CAN-FD adapter implementing ICanBus.
/// Wraps the Zephyr CAN driver API for send/receive with callbacks.
/// Supports multiple RX listeners via addRxCallback().
/// Thread-safety: addRxCallback() must only be called during lifecycle init
/// (before the CAN filter is active). rxDispatch() runs in ISR context and
/// iterates the callback list without locking.
class CanAdapter : public ports::ICanBus {
public:
    explicit CanAdapter(const struct device* can_dev);

    bool configure(can_mode_t mode = CAN_MODE_FD);

    bool send(const ports::CanFrame& frame) override;
    void addRxCallback(ports::CanRxCallback callback) override;

private:
    void ensureFilter();
    static void rxDispatch(const struct device* dev,
                           struct can_frame* frame, void* user_data);

    const struct device* can_dev_;
    std::vector<ports::CanRxCallback> rx_callbacks_;
    int filter_id_{-1};
};

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
