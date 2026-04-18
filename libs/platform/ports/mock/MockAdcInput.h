#pragma once

#include <gmock/gmock.h>

#include "ports/IAdcInput.h"

namespace body_ecu::mocks {

class MockAdcInput : public ports::IAdcInput {
public:
    MOCK_METHOD(int32_t, read, (uint8_t channel), (override));
};

}  // namespace body_ecu::mocks
