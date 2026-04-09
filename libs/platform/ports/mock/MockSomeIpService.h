#pragma once

#include <gmock/gmock.h>

#include "ports/ISomeIpService.h"

namespace body_ecu::mocks {

class MockSomeIpService : public ports::ISomeIpService {
public:
    MOCK_METHOD(void, registerMethod,
                (uint16_t service_id, uint16_t method_id,
                 ports::MethodHandler handler),
                (override));
    MOCK_METHOD(void, registerEvent,
                (uint16_t service_id, uint16_t event_id,
                 uint16_t eventgroup_id),
                (override));
    MOCK_METHOD(void, sendEvent,
                (uint16_t service_id, uint16_t event_id,
                 const std::vector<uint8_t>& payload),
                (override));
    MOCK_METHOD(void, sendResponse, (const ports::SomeIpMessage& response),
                (override));
};

}  // namespace body_ecu::mocks
