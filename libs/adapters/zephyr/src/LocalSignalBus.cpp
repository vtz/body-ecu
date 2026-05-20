#include "zephyr_adapters/LocalSignalBus.h"

namespace body_ecu::adapters {

bool LocalSignalBus::publish(const std::string& path,
                             const ports::SignalValue& value) {
    std::vector<ports::SignalCallback> cbs;
    {
        k_mutex_lock(&mutex_, K_FOREVER);
        store_[path] = value;
        auto it = subscribers_.find(path);
        if (it != subscribers_.end()) {
            cbs = it->second;
        }
        k_mutex_unlock(&mutex_);
    }
    for (auto& cb : cbs) {
        cb(path, value);
    }
    return true;
}

void LocalSignalBus::subscribe(const std::string& path,
                               ports::SignalCallback callback) {
    k_mutex_lock(&mutex_, K_FOREVER);
    subscribers_[path].push_back(std::move(callback));
    k_mutex_unlock(&mutex_);
}

std::optional<ports::SignalValue> LocalSignalBus::get(
    const std::string& path) const {
    k_mutex_lock(&mutex_, K_FOREVER);
    auto it = store_.find(path);
    if (it == store_.end()) {
        k_mutex_unlock(&mutex_);
        return std::nullopt;
    }
    auto val = it->second;
    k_mutex_unlock(&mutex_);
    return val;
}

}  // namespace body_ecu::adapters
