#pragma once

#ifndef BUILD_TESTS

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <cstdint>
#include <vector>

#include "ports/IGpioPort.h"

namespace body_ecu::adapters {

/// Zephyr GPIO adapter implementing IGpioPort.
/// Uses gpio_dt_spec for correct active-level handling.
/// On NUCLEO-H753ZI, pins 0-2 map to on-board LEDs
/// (green=LD1, yellow=LD2, red=LD3).
class GpioAdapter : public ports::IGpioPort {
public:
    explicit GpioAdapter(const std::vector<struct gpio_dt_spec>& specs);

    bool configure();

    void write(uint32_t pin, bool value) override;
    bool read(uint32_t pin) const override;

private:
    std::vector<struct gpio_dt_spec> specs_;
};

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
