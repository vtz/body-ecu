#ifndef BUILD_TESTS

#include "zephyr_adapters/GpioAdapter.h"

#include <zephyr/sys/printk.h>

namespace body_ecu::adapters {

GpioAdapter::GpioAdapter(const std::vector<struct gpio_dt_spec>& specs)
    : specs_(specs) {}

bool GpioAdapter::configure() {
    for (size_t i = 0; i < specs_.size(); i++) {
        if (!gpio_is_ready_dt(&specs_[i])) {
            printk("[gpio] Port not ready for LED %zu (pin %d)\n", i, specs_[i].pin);
            return false;
        }
        int ret = gpio_pin_configure_dt(&specs_[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            printk("[gpio] Failed to configure LED %zu (pin %d): %d\n", i, specs_[i].pin, ret);
            return false;
        }
    }
    return true;
}

void GpioAdapter::write(uint32_t pin, bool value) {
    if (pin >= specs_.size()) return;
    if (!gpio_is_ready_dt(&specs_[pin])) return;
    gpio_pin_set_dt(&specs_[pin], value ? 1 : 0);
}

bool GpioAdapter::read(uint32_t pin) const {
    if (pin >= specs_.size()) return false;
    if (!gpio_is_ready_dt(&specs_[pin])) return false;
    return gpio_pin_get_dt(&specs_[pin]) != 0;
}

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
