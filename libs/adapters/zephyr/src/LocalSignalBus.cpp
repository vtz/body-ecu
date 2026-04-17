#include "zephyr_adapters/LocalSignalBus.h"

namespace body_ecu::adapters {

bool LocalSignalBus::publish(const std::string& path,
                             const ports::SignalValue& value) {
    store_[path] = value;
    auto it = subscribers_.find(path);
    if (it != subscribers_.end()) {
        for (auto& cb : it->second) {
            cb(path, value);
        }
    }
    return true;
}

void LocalSignalBus::subscribe(const std::string& path,
                               ports::SignalCallback callback) {
    subscribers_[path].push_back(std::move(callback));
}

std::optional<ports::SignalValue> LocalSignalBus::get(
    const std::string& path) const {
    auto it = store_.find(path);
    if (it == store_.end()) return std::nullopt;
    return it->second;
}

}  // namespace body_ecu::adapters
