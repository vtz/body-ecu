#pragma once

#include "SomeIpSystem.h"
#include "can_gateway/CanGateway.h"
#include "ports/ICanBus.h"

namespace body_ecu::adapters {

/// AsyncLifecycleComponent wrapper that owns a CanGateway.
class CanGatewaySystem {
public:
    CanGatewaySystem(ports::ICanBus& can, SomeIpSystem& someip);

    void addMapping(const platform::ServiceMapping& mapping);

    void init();
    void run();
    void shutdown();

    platform::CanGateway& gateway() { return gateway_; }

private:
    platform::CanGateway gateway_;
};

}  // namespace body_ecu::adapters
