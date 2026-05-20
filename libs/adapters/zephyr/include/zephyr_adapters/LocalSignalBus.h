#pragma once

#include <map>
#include <string>
#include <vector>

#include <zephyr/kernel.h>

#include "ports/ISignalBus.h"

namespace body_ecu::adapters {

/// Lightweight in-process ISignalBus for MCU (Zephyr) builds.
/// Thread-safe: all operations are guarded by a k_mutex.
class LocalSignalBus : public ports::ISignalBus {
public:
    LocalSignalBus() { k_mutex_init(&mutex_); }

    bool publish(const std::string& path,
                 const ports::SignalValue& value) override;
    void subscribe(const std::string& path,
                   ports::SignalCallback callback) override;
    std::optional<ports::SignalValue> get(
        const std::string& path) const override;

private:
    mutable struct k_mutex mutex_;
    std::map<std::string, ports::SignalValue> store_;
    std::map<std::string, std::vector<ports::SignalCallback>> subscribers_;
};

}  // namespace body_ecu::adapters
