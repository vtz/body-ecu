#ifndef BUILD_TESTS

#include "zephyr_adapters/ButtonAdapter.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(button_adapter, LOG_LEVEL_INF);

namespace body_ecu::adapters {

ButtonAdapter::ButtonAdapter(const struct device* port, gpio_pin_t pin)
    : port_(port), pin_(pin) {}

bool ButtonAdapter::configure() {
    if (!device_is_ready(port_)) {
        LOG_ERR("Button GPIO port not ready");
        return false;
    }

    int ret = gpio_pin_configure(port_, pin_, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) {
        LOG_ERR("Failed to configure button pin: %d", ret);
        return false;
    }

    ret = gpio_pin_interrupt_configure(port_, pin_,
                                       GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure button interrupt: %d", ret);
        return false;
    }

    gpio_init_callback(&cb_data_, isrHandler, BIT(pin_));
    gpio_add_callback(port_, &cb_data_);
    return true;
}

void ButtonAdapter::onPress(ports::ButtonCallback callback) {
    callback_ = std::move(callback);
}

void ButtonAdapter::isrHandler(const struct device* /*dev*/,
                               struct gpio_callback* cb, uint32_t /*pins*/) {
    auto* self = CONTAINER_OF(cb, ButtonAdapter, cb_data_);
    if (self->callback_) {
        self->callback_();
    }
}

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
