#pragma once

#include <gmock/gmock.h>

#include "ports/IModeObserver.h"

namespace body_ecu::mocks {

class MockModeObserver : public ports::IModeObserver {
public:
    MOCK_METHOD(void, onModeChanged,
                (ports::VehicleMode old_mode, ports::VehicleMode new_mode),
                (override));
};

}  // namespace body_ecu::mocks
