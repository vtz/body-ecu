#ifndef BUILD_TESTS

#include "zephyr_adapters/CanAdapter.h"

#include <zephyr/logging/log.h>

#include <cstring>

LOG_MODULE_REGISTER(can_adapter, LOG_LEVEL_INF);

namespace body_ecu::adapters {

CanAdapter::CanAdapter(const struct device* can_dev) : can_dev_(can_dev) {}

bool CanAdapter::configure(can_mode_t mode) {
    if (!device_is_ready(can_dev_)) {
        LOG_ERR("CAN device not ready");
        return false;
    }

    int ret = can_set_mode(can_dev_, mode);
    if (ret < 0) {
        LOG_ERR("Failed to set CAN mode: %d", ret);
        return false;
    }

    ret = can_start(can_dev_);
    if (ret < 0) {
        LOG_ERR("Failed to start CAN: %d", ret);
        return false;
    }

    return true;
}

bool CanAdapter::send(const ports::CanFrame& frame) {
    struct can_frame zframe {};
    zframe.id = frame.id;
    zframe.dlc = frame.dlc;
    zframe.flags = (frame.dlc > 8) ? CAN_FRAME_FDF : 0;
    std::memcpy(zframe.data, frame.data, frame.dlc);

    int ret = can_send(can_dev_, &zframe, K_MSEC(100), nullptr, nullptr);
    if (ret < 0) {
        LOG_ERR("CAN send failed: %d", ret);
        return false;
    }
    return true;
}

void CanAdapter::setRxCallback(ports::CanRxCallback callback) {
    rx_callback_ = std::move(callback);

    struct can_filter filter {};
    filter.id = 0;
    filter.mask = 0;  // Accept all

    if (filter_id_ >= 0) {
        can_remove_rx_filter(can_dev_, filter_id_);
    }

    filter_id_ = can_add_rx_filter(can_dev_, rxDispatch, this, &filter);
    if (filter_id_ < 0) {
        LOG_ERR("Failed to add CAN RX filter: %d", filter_id_);
    }
}

void CanAdapter::rxDispatch(const struct device* /*dev*/,
                            struct can_frame* frame, void* user_data) {
    auto* self = static_cast<CanAdapter*>(user_data);
    if (!self->rx_callback_) return;

    ports::CanFrame pf;
    pf.id = frame->id;
    pf.dlc = frame->dlc;
    std::memcpy(pf.data, frame->data, frame->dlc);
    self->rx_callback_(pf);
}

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
