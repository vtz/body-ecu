#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "ports/IGpioPort.h"

namespace body_ecu::adapters {

/// Linux/POSIX GPIO adapter that logs pin state to stdout.
/// Simulates GPIO behavior for desktop development and testing.
/// On a real Linux HPC, replace with sysfs/gpiod implementation.
class ConsoleGpioAdapter : public ports::IGpioPort {
public:
    static constexpr size_t kMaxPins = 32;

    explicit ConsoleGpioAdapter(
        const std::vector<std::string>& pin_names = {"LED_GREEN", "LED_YELLOW", "LED_RED"});

    void write(uint32_t pin, bool value) override;
    bool read(uint32_t pin) const override;

private:
    std::vector<std::string> pin_names_;
    std::array<bool, kMaxPins> states_{};
};

}  // namespace body_ecu::adapters
