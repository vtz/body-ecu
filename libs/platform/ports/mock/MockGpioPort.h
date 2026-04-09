#pragma once

#include <gmock/gmock.h>

#include "ports/IGpioPort.h"

namespace body_ecu::mocks {

class MockGpioPort : public ports::IGpioPort {
public:
    MOCK_METHOD(void, write, (uint32_t pin, bool value), (override));
    MOCK_METHOD(bool, read, (uint32_t pin), (const, override));
};

}  // namespace body_ecu::mocks
