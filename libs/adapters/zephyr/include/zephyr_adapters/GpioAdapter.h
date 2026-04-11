#pragma once

#ifndef BUILD_TESTS

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <array>
#include <cstdint>
#include <vector>

#include "ports/IGpioPort.h"

namespace body_ecu::adapters {

/// Zephyr GPIO adapter implementing IGpioPort.
/// Maps virtual pin numbers to Zephyr GPIO device/pin pairs.
/// On NUCLEO-H755ZI-Q, pins 0-2 map to on-board LEDs
/// (green=LD1, yellow=LD2, red=LD3).
class GpioAdapter : public ports::IGpioPort {
public:
    struct PinMapping {
        const struct device* port;
        gpio_pin_t pin;
        gpio_flags_t flags;
    };

    explicit GpioAdapter(const std::vector<PinMapping>& mappings);

    bool configure();

    void write(uint32_t pin, bool value) override;
    bool read(uint32_t pin) const override;

private:
    std::vector<PinMapping> mappings_;
};

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
