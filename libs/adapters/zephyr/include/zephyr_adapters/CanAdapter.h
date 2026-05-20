#pragma once

#ifndef BUILD_TESTS

#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>

#include <vector>

#include "ports/ICanBus.h"

namespace body_ecu::adapters {

/// Zephyr CAN-FD adapter implementing ICanBus.
/// Wraps the Zephyr CAN driver API for send/receive with callbacks.
///
/// Usage: register all callbacks via addRxCallback() during init, then call
/// startReceiving() once. After that point the callback list is frozen and
/// rxDispatch() iterates it from ISR context without locking.
class CanAdapter : public ports::ICanBus {
public:
    explicit CanAdapter(const struct device* can_dev);

    bool configure(can_mode_t mode = CAN_MODE_FD);

    bool send(const ports::CanFrame& frame) override;
    void addRxCallback(ports::CanRxCallback callback) override;

    /// Install the CAN RX filter after all callbacks are registered.
    /// Must be called exactly once, after all addRxCallback() calls.
    void startReceiving();

private:
    static void rxDispatch(const struct device* dev,
                           struct can_frame* frame, void* user_data);

    const struct device* can_dev_;
    std::vector<ports::CanRxCallback> rx_callbacks_;
    int filter_id_{-1};
};

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
