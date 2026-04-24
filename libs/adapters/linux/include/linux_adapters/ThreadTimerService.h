#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "ports/ITimerService.h"

namespace body_ecu::adapters {

class ThreadTimerService : public ports::ITimerService {
public:
    ~ThreadTimerService();

    ports::TimerId startPeriodic(uint32_t interval_ms,
                                 ports::TimerCallback callback) override;
    ports::TimerId startOneShot(uint32_t delay_ms,
                                ports::TimerCallback callback) override;
    void cancel(ports::TimerId id) override;

    void stopAll();

private:
    struct TimerSlot {
        std::thread thread;
        std::atomic<bool> running{false};
        std::mutex mtx;
        std::condition_variable cv;
    };

    std::mutex slots_mtx_;
    std::vector<std::unique_ptr<TimerSlot>> slots_;
    ports::TimerId next_id_{0};
};

}  // namespace body_ecu::adapters
