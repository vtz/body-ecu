#pragma once

#include "lifecycle/ILifecycleComponent.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace lifecycle {

/// Lightweight lifecycle manager that follows the openbsw LifecycleManager
/// pattern: components are registered at numbered run levels and transitioned
/// (INIT → RUN) in ascending level order, or shut down in descending order.
///
/// All transitions run synchronously in the caller's thread. When the real
/// openbsw LifecycleManager is available (Zephyr + async framework), this
/// class can be replaced by lifecycle::declare::LifecycleManager.
class LifecycleManager {
public:
    void addComponent(const char* name, ILifecycleComponent& component,
                      uint8_t level = 0);

    /// Transition all components up to (and including) the given level.
    /// Each level is fully initialised and run before the next level starts.
    void transitionToLevel(uint8_t level);

    /// Shut down all running components in reverse level order.
    void shutdownAll();

    uint8_t currentLevel() const { return current_level_; }

private:
    struct Entry {
        const char* name;
        ILifecycleComponent* component;
        uint8_t level;
    };

    std::vector<Entry> components_;
    uint8_t current_level_{0};
    bool running_{false};
};

}  // namespace lifecycle
