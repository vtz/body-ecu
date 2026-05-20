#pragma once

#include <map>
#include <vector>

#include "can_gateway/ServiceMapping.h"
#include "ports/ICanBus.h"
#include "ports/ISomeIpService.h"

namespace body_ecu::platform {

class CanGateway {
public:
    CanGateway(ports::ICanBus& can, ports::ISomeIpService& someip);

    void addMapping(const ServiceMapping& mapping);
    void start();
    void stop();
    bool isRunning() const { return running_; }

    void onSomeIpMessage(const ports::SomeIpMessage& msg);
    void onCanFrame(const ports::CanFrame& frame);

private:
    ports::ICanBus& can_;
    ports::ISomeIpService& someip_;
    std::vector<ServiceMapping> mappings_;
    std::map<uint32_t, const ServiceMapping*> someip_to_can_index_;
    std::map<uint32_t, const ServiceMapping*> can_to_someip_index_;
    bool running_{false};
    bool rx_registered_{false};

    void buildIndices();
    static uint32_t someipKey(uint16_t service_id, uint16_t method_id) {
        return (static_cast<uint32_t>(service_id) << 16) | method_id;
    }
};

}  // namespace body_ecu::platform
