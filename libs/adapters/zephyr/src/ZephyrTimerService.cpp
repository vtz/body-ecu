#ifndef BUILD_TESTS

#include "zephyr_adapters/ZephyrTimerService.h"

#include <zephyr/sys/printk.h>

namespace body_ecu::adapters {

ports::TimerId ZephyrTimerService::startPeriodic(
    uint32_t interval_ms, ports::TimerCallback callback) {
    for (size_t i = 0; i < kMaxTimers; ++i) {
        if (!slots_[i].active) {
            slots_[i].active = true;
            slots_[i].periodic = true;
            slots_[i].callback = std::move(callback);
            k_work_init(&slots_[i].work, workHandler);
            k_timer_init(&slots_[i].timer, expiryHandler, nullptr);
            slots_[i].timer.user_data = &slots_[i];
            k_timer_start(&slots_[i].timer, K_MSEC(interval_ms),
                          K_MSEC(interval_ms));
            return static_cast<ports::TimerId>(i);
        }
    }
    printk("[timer] No free timer slots\n");
    return static_cast<ports::TimerId>(kMaxTimers);
}

ports::TimerId ZephyrTimerService::startOneShot(
    uint32_t delay_ms, ports::TimerCallback callback) {
    for (size_t i = 0; i < kMaxTimers; ++i) {
        if (!slots_[i].active) {
            slots_[i].active = true;
            slots_[i].periodic = false;
            slots_[i].callback = std::move(callback);
            k_work_init(&slots_[i].work, workHandler);
            k_timer_init(&slots_[i].timer, expiryHandler, nullptr);
            slots_[i].timer.user_data = &slots_[i];
            k_timer_start(&slots_[i].timer, K_MSEC(delay_ms), K_NO_WAIT);
            return static_cast<ports::TimerId>(i);
        }
    }
    printk("[timer] No free timer slots\n");
    return static_cast<ports::TimerId>(kMaxTimers);
}

void ZephyrTimerService::cancel(ports::TimerId id) {
    auto idx = static_cast<size_t>(id);
    if (idx < kMaxTimers && slots_[idx].active) {
        k_timer_stop(&slots_[idx].timer);
        slots_[idx].active = false;
        slots_[idx].callback = nullptr;
    }
}

void ZephyrTimerService::expiryHandler(struct k_timer* timer) {
    auto* slot = static_cast<TimerSlot*>(timer->user_data);
    if (slot && slot->active) {
        k_work_submit(&slot->work);
    }
}

void ZephyrTimerService::workHandler(struct k_work* work) {
    auto* slot = CONTAINER_OF(work, TimerSlot, work);
    if (slot && slot->callback) {
        slot->callback();
        if (!slot->periodic) {
            slot->active = false;
        }
    }
}

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
