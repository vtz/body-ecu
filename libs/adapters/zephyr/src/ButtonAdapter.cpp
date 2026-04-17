#ifndef BUILD_TESTS

#include "zephyr_adapters/ButtonAdapter.h"

#include <zephyr/sys/printk.h>

namespace body_ecu::adapters {

ButtonAdapter::ButtonAdapter(const struct gpio_dt_spec& spec)
    : spec_(spec) {}

bool ButtonAdapter::configure() {
    if (!gpio_is_ready_dt(&spec_)) {
        printk("[button] GPIO port not ready\n");
        return false;
    }

    k_work_init(&work_, workHandler);

    int ret = gpio_pin_configure_dt(&spec_, GPIO_INPUT);
    if (ret < 0) {
        printk("[button] Failed to configure pin: %d\n", ret);
        return false;
    }

    ret = gpio_pin_interrupt_configure_dt(&spec_, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("[button] Failed to configure interrupt: %d\n", ret);
        return false;
    }

    gpio_init_callback(&cb_data_, isrHandler, BIT(spec_.pin));
    gpio_add_callback(spec_.port, &cb_data_);
    printk("[button] Configured on pin %d (flags=0x%x)\n", spec_.pin, spec_.dt_flags);
    return true;
}

void ButtonAdapter::onPress(ports::ButtonCallback callback) {
    callback_ = std::move(callback);
}

void ButtonAdapter::isrHandler(const struct device* /*dev*/,
                               struct gpio_callback* cb, uint32_t /*pins*/) {
    auto* self = CONTAINER_OF(cb, ButtonAdapter, cb_data_);
    k_work_submit(&self->work_);
}

void ButtonAdapter::workHandler(struct k_work* work) {
    auto* self = CONTAINER_OF(work, ButtonAdapter, work_);
    if (self->callback_) {
        self->callback_();
    }
}

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
