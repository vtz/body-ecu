#pragma once

#include <cstdint>
#include <functional>

namespace body_ecu::ports {

using TimerCallback = std::function<void()>;
using TimerId = uint32_t;

class ITimerService {
public:
    virtual ~ITimerService() = default;
    virtual TimerId startPeriodic(uint32_t interval_ms,
                                  TimerCallback callback) = 0;
    virtual TimerId startOneShot(uint32_t delay_ms,
                                 TimerCallback callback) = 0;
    virtual void cancel(TimerId id) = 0;
};

}  // namespace body_ecu::ports
