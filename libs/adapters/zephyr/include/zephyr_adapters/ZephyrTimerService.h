#pragma once

#ifndef BUILD_TESTS

#include <zephyr/kernel.h>

#include <array>
#include <cstdint>

#include "ports/ITimerService.h"

namespace body_ecu::adapters {

class ZephyrTimerService : public ports::ITimerService {
public:
    static constexpr size_t kMaxTimers = 8;

    ports::TimerId startPeriodic(uint32_t interval_ms,
                                 ports::TimerCallback callback) override;
    ports::TimerId startOneShot(uint32_t delay_ms,
                                ports::TimerCallback callback) override;
    void cancel(ports::TimerId id) override;

private:
    struct TimerSlot {
        struct k_timer timer;
        ports::TimerCallback callback;
        bool active{false};
        bool periodic{false};
    };

    static void expiryHandler(struct k_timer* timer);

    std::array<TimerSlot, kMaxTimers> slots_{};
    ports::TimerId next_id_{0};
};

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
