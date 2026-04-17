#pragma once

#include <gmock/gmock.h>

#include "ports/ICloudTransport.h"

namespace body_ecu::mocks {

class MockCloudTransport : public ports::ICloudTransport {
public:
    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, publish,
                (const std::string& subject,
                 const std::vector<uint8_t>& data),
                (override));
    MOCK_METHOD(void, subscribe,
                (const std::string& subject,
                 ports::CloudMessageCallback callback),
                (override));
};

}  // namespace body_ecu::mocks
