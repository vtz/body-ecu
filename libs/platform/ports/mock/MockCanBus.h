#pragma once

#include <gmock/gmock.h>

#include "ports/ICanBus.h"

namespace body_ecu::mocks {

class MockCanBus : public ports::ICanBus {
public:
    MOCK_METHOD(bool, send, (const ports::CanFrame& frame), (override));
    MOCK_METHOD(void, addRxCallback, (ports::CanRxCallback callback),
                (override));
    MOCK_METHOD(void, setRxCallback, (ports::CanRxCallback callback),
                (override));
};

}  // namespace body_ecu::mocks
