#pragma once

#include <gmock/gmock.h>

#include "ports/IButtonInput.h"

namespace body_ecu::mocks {

class MockButtonInput : public ports::IButtonInput {
public:
    MOCK_METHOD(void, onPress, (ports::ButtonCallback callback), (override));
};

}  // namespace body_ecu::mocks
