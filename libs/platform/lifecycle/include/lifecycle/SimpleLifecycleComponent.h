#pragma once

#include "lifecycle/ILifecycleComponent.h"

namespace lifecycle {

/// Lightweight shim for builds without OpenBSW.
/// Provides the same interface as openbsw's SimpleLifecycleComponent
/// so that SomeIpSystem (and other adapters) compile unchanged.
class SimpleLifecycleComponent : public ILifecycleComponent {
public:
    SimpleLifecycleComponent& operator=(SimpleLifecycleComponent const&) = delete;

protected:
    ~SimpleLifecycleComponent() = default;
    void transitionDone() {}
};

}  // namespace lifecycle
