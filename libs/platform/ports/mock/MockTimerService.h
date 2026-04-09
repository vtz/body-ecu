#pragma once

#include <gmock/gmock.h>

#include "ports/ITimerService.h"

namespace body_ecu::mocks {

class MockTimerService : public ports::ITimerService {
public:
    MOCK_METHOD(ports::TimerId, startPeriodic,
                (uint32_t interval_ms, ports::TimerCallback callback),
                (override));
    MOCK_METHOD(ports::TimerId, startOneShot,
                (uint32_t delay_ms, ports::TimerCallback callback),
                (override));
    MOCK_METHOD(void, cancel, (ports::TimerId id), (override));
};

}  // namespace body_ecu::mocks
