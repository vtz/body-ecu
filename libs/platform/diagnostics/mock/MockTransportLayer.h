#pragma once

#include <gmock/gmock.h>

#include "diagnostics/ITransportLayer.h"

namespace body_ecu::mocks {

class MockTransportLayer : public platform::ITransportLayer {
public:
    MOCK_METHOD(void, setRequestHandler,
                (platform::DiagRequestHandler handler), (override));
    MOCK_METHOD(void, sendResponse,
                (const platform::DiagResponse& response), (override));
};

}  // namespace body_ecu::mocks
