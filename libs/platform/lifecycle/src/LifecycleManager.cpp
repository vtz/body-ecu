#include "lifecycle/LifecycleManager.h"

#include <algorithm>

namespace lifecycle {

void LifecycleManager::addComponent(const char* name,
                                    ILifecycleComponent& component,
                                    uint8_t level) {
    components_.push_back({name, &component, level});
}

void LifecycleManager::transitionToLevel(uint8_t level) {
    std::stable_sort(components_.begin(), components_.end(),
                     [](const Entry& a, const Entry& b) {
                         return a.level < b.level;
                     });

    for (uint8_t lv = 1; lv <= level; ++lv) {
        for (auto& e : components_) {
            if (e.level != lv) continue;
            std::printf("[lifecycle] INIT  level=%u  %s\n", lv, e.name);
            e.component->init();
        }
        for (auto& e : components_) {
            if (e.level != lv) continue;
            std::printf("[lifecycle] RUN   level=%u  %s\n", lv, e.name);
            e.component->run();
        }
    }

    current_level_ = level;
    running_ = true;
}

void LifecycleManager::shutdownAll() {
    if (!running_) return;

    for (uint8_t lv = current_level_; lv >= 1; --lv) {
        for (auto it = components_.rbegin(); it != components_.rend(); ++it) {
            if (it->level != lv) continue;
            std::printf("[lifecycle] SHUT  level=%u  %s\n", lv, it->name);
            it->component->shutdown();
        }
    }

    current_level_ = 0;
    running_ = false;
}

}  // namespace lifecycle
