#pragma once

#include <cstdint>

namespace body_ecu::ports {

class IAdcInput {
public:
    virtual ~IAdcInput() = default;
    virtual int32_t read(uint8_t channel) = 0;
};

}  // namespace body_ecu::ports
