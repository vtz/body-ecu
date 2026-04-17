#pragma once

#ifndef BUILD_TESTS

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "ports/IButtonInput.h"

namespace body_ecu::adapters {

/// Zephyr GPIO interrupt adapter implementing IButtonInput.
/// Uses the full gpio_dt_spec to correctly handle active-high/low polarity.
class ButtonAdapter : public ports::IButtonInput {
public:
    explicit ButtonAdapter(const struct gpio_dt_spec& spec);

    bool configure();

    void onPress(ports::ButtonCallback callback) override;

private:
    static void isrHandler(const struct device* dev,
                           struct gpio_callback* cb, uint32_t pins);
    static void workHandler(struct k_work* work);

    struct gpio_dt_spec spec_;
    struct gpio_callback cb_data_;
    struct k_work work_;
    ports::ButtonCallback callback_;
};

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
