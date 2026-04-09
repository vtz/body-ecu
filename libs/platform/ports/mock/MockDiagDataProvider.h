#pragma once

#include <gmock/gmock.h>

#include "ports/IDiagDataProvider.h"

namespace body_ecu::mocks {

class MockDiagDataProvider : public ports::IDiagDataProvider {
public:
    MOCK_METHOD(std::optional<ports::DiagData>, readData, (uint16_t did),
                (const, override));
    MOCK_METHOD(bool, ioControl,
                (uint16_t did, const std::vector<uint8_t>& control_param),
                (override));
};

}  // namespace body_ecu::mocks
