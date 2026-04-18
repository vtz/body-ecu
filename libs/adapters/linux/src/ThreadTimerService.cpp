#include "linux_adapters/ThreadTimerService.h"

#include <chrono>

namespace body_ecu::adapters {

ThreadTimerService::~ThreadTimerService() {
    stopAll();
}

ports::TimerId ThreadTimerService::startPeriodic(
    uint32_t interval_ms, ports::TimerCallback callback) {
    std::lock_guard<std::mutex> lock(slots_mtx_);
    auto id = next_id_++;
    auto slot = std::make_unique<TimerSlot>();
    slot->running = true;
    auto* slot_ptr = slot.get();
    slot->thread = std::thread(
        [slot_ptr, interval_ms, cb = std::move(callback)]() {
            while (slot_ptr->running) {
                std::unique_lock<std::mutex> lk(slot_ptr->mtx);
                if (slot_ptr->cv.wait_for(
                        lk, std::chrono::milliseconds(interval_ms),
                        [&] { return !slot_ptr->running.load(); })) {
                    break;
                }
                if (slot_ptr->running && cb) {
                    cb();
                }
            }
        });
    slots_.push_back(std::move(slot));
    return id;
}

ports::TimerId ThreadTimerService::startOneShot(
    uint32_t delay_ms, ports::TimerCallback callback) {
    std::lock_guard<std::mutex> lock(slots_mtx_);
    auto id = next_id_++;
    auto slot = std::make_unique<TimerSlot>();
    slot->running = true;
    auto* slot_ptr = slot.get();
    slot->thread = std::thread(
        [slot_ptr, delay_ms, cb = std::move(callback)]() {
            std::unique_lock<std::mutex> lk(slot_ptr->mtx);
            if (!slot_ptr->cv.wait_for(
                    lk, std::chrono::milliseconds(delay_ms),
                    [&] { return !slot_ptr->running.load(); })) {
                if (slot_ptr->running && cb) {
                    cb();
                }
            }
            slot_ptr->running = false;
        });
    slots_.push_back(std::move(slot));
    return id;
}

void ThreadTimerService::cancel(ports::TimerId id) {
    std::lock_guard<std::mutex> lock(slots_mtx_);
    auto idx = static_cast<size_t>(id);
    if (idx < slots_.size() && slots_[idx] && slots_[idx]->running) {
        slots_[idx]->running = false;
        slots_[idx]->cv.notify_all();
        if (slots_[idx]->thread.joinable()) {
            slots_[idx]->thread.join();
        }
    }
}

void ThreadTimerService::stopAll() {
    std::lock_guard<std::mutex> lock(slots_mtx_);
    for (auto& slot : slots_) {
        if (slot && slot->running) {
            slot->running = false;
            slot->cv.notify_all();
        }
    }
    for (auto& slot : slots_) {
        if (slot && slot->thread.joinable()) {
            slot->thread.join();
        }
    }
    slots_.clear();
}

}  // namespace body_ecu::adapters
