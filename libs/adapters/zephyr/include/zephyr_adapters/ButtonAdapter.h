#pragma once

#ifndef BUILD_TESTS

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "ports/IButtonInput.h"

namespace body_ecu::adapters {

/// Zephyr GPIO interrupt adapter implementing IButtonInput.
/// Configures a GPIO pin with interrupt on rising edge for button press.
class ButtonAdapter : public ports::IButtonInput {
public:
    ButtonAdapter(const struct device* port, gpio_pin_t pin);

    bool configure();

    void onPress(ports::ButtonCallback callback) override;

private:
    static void isrHandler(const struct device* dev,
                           struct gpio_callback* cb, uint32_t pins);

    const struct device* port_;
    gpio_pin_t pin_;
    struct gpio_callback cb_data_;
    ports::ButtonCallback callback_;
};

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
