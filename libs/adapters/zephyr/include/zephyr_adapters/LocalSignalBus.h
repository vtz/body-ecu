#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "ports/ISignalBus.h"

namespace body_ecu::adapters {

/// Lightweight in-process ISignalBus for MCU (Zephyr) and POSIX builds.
/// Thread-safe: all operations are guarded by a mutex.
class LocalSignalBus : public ports::ISignalBus {
public:
    bool publish(const std::string& path,
                 const ports::SignalValue& value) override;
    void subscribe(const std::string& path,
                   ports::SignalCallback callback) override;
    std::optional<ports::SignalValue> get(
        const std::string& path) const override;

private:
    mutable std::mutex mutex_;
    std::map<std::string, ports::SignalValue> store_;
    std::map<std::string, std::vector<ports::SignalCallback>> subscribers_;
};

}  // namespace body_ecu::adapters
