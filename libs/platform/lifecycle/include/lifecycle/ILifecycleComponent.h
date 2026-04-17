#pragma once

namespace lifecycle {

/// Lifecycle component interface following the openbsw pattern.
///
/// Components implement init/run/shutdown transitions. A LifecycleManager
/// calls these in order, grouped by run level. When the real openbsw
/// LifecycleManager is wired (Zephyr builds), an adapter bridges this
/// interface to lifecycle::SimpleLifecycleComponent.
class ILifecycleComponent {
public:
    virtual ~ILifecycleComponent() = default;

    virtual void init() = 0;
    virtual void run() = 0;
    virtual void shutdown() = 0;
};

}  // namespace lifecycle
