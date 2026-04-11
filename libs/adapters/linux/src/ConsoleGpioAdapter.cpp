#include "linux_adapters/ConsoleGpioAdapter.h"

#include <cstdio>

namespace body_ecu::adapters {

ConsoleGpioAdapter::ConsoleGpioAdapter(
    const std::vector<std::string>& pin_names)
    : pin_names_(pin_names) {}

void ConsoleGpioAdapter::write(uint32_t pin, bool value) {
    if (pin >= kMaxPins) return;
    states_[pin] = value;

    const char* name = (pin < pin_names_.size())
                           ? pin_names_[pin].c_str()
                           : "PIN_?";
    std::printf("[GPIO] %s (pin %u) -> %s\n", name, pin,
                value ? "ON" : "OFF");
}

bool ConsoleGpioAdapter::read(uint32_t pin) const {
    if (pin >= kMaxPins) return false;
    return states_[pin];
}

}  // namespace body_ecu::adapters
