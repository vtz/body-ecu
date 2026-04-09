#ifndef BUILD_TESTS

#include "zephyr_adapters/GpioAdapter.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gpio_adapter, LOG_LEVEL_INF);

namespace body_ecu::adapters {

GpioAdapter::GpioAdapter(const std::vector<PinMapping>& mappings)
    : mappings_(mappings) {}

bool GpioAdapter::configure() {
    for (const auto& m : mappings_) {
        if (!device_is_ready(m.port)) {
            LOG_ERR("GPIO port not ready for pin %d", m.pin);
            return false;
        }
        int ret = gpio_pin_configure(m.port, m.pin,
                                     m.flags | GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure GPIO pin %d: %d", m.pin, ret);
            return false;
        }
    }
    return true;
}

void GpioAdapter::write(uint32_t pin, bool value) {
    if (pin >= mappings_.size()) return;
    gpio_pin_set(mappings_[pin].port, mappings_[pin].pin,
                 value ? 1 : 0);
}

bool GpioAdapter::read(uint32_t pin) const {
    if (pin >= mappings_.size()) return false;
    return gpio_pin_get(mappings_[pin].port, mappings_[pin].pin) != 0;
}

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
