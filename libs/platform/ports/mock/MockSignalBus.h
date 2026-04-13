#pragma once

#include <gmock/gmock.h>

#include "ports/ISignalBus.h"

namespace body_ecu::mocks {

class MockSignalBus : public ports::ISignalBus {
public:
    MOCK_METHOD(bool, publish,
                (const std::string& path, const ports::SignalValue& value),
                (override));
    MOCK_METHOD(void, subscribe,
                (const std::string& path, ports::SignalCallback callback),
                (override));
    MOCK_METHOD(std::optional<ports::SignalValue>, get,
                (const std::string& path), (const, override));
};

}  // namespace body_ecu::mocks
