#pragma once

#include "SomeIpSystem.h"
#include "can_gateway/CanGateway.h"
#include "lifecycle/ILifecycleComponent.h"
#include "ports/ICanBus.h"

namespace body_ecu::adapters {

class CanGatewaySystem : public lifecycle::ILifecycleComponent {
public:
    CanGatewaySystem(ports::ICanBus& can, SomeIpSystem& someip);

    void addMapping(const platform::ServiceMapping& mapping);

    void init() override;
    void run() override;
    void shutdown() override;

    platform::CanGateway& gateway() { return gateway_; }

private:
    platform::CanGateway gateway_;
};

}  // namespace body_ecu::adapters
