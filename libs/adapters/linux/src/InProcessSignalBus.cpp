#include "linux_adapters/InProcessSignalBus.h"

namespace body_ecu::adapters {

bool InProcessSignalBus::publish(const std::string& path,
                                 const ports::SignalValue& value) {
    std::vector<ports::SignalCallback> callbacks;
    {
        std::lock_guard lock(mutex_);
        store_[path] = value;
        auto it = subscribers_.find(path);
        if (it != subscribers_.end()) {
            callbacks = it->second;
        }
    }
    for (auto& cb : callbacks) {
        cb(path, value);
    }
    return true;
}

void InProcessSignalBus::subscribe(const std::string& path,
                                   ports::SignalCallback callback) {
    std::lock_guard lock(mutex_);
    subscribers_[path].push_back(std::move(callback));
}

std::optional<ports::SignalValue> InProcessSignalBus::get(
    const std::string& path) const {
    std::lock_guard lock(mutex_);
    auto it = store_.find(path);
    if (it == store_.end()) return std::nullopt;
    return it->second;
}

}  // namespace body_ecu::adapters
