#pragma once

#include <cstdint>

namespace body_ecu::ports {

class IGpioPort {
public:
    virtual ~IGpioPort() = default;
    virtual void write(uint32_t pin, bool value) = 0;
    virtual bool read(uint32_t pin) const = 0;
};

}  // namespace body_ecu::ports
