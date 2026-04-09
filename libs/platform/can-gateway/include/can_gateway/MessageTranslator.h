#pragma once

#include <vector>

#include "ports/ICanBus.h"
#include "ports/ISomeIpService.h"

namespace body_ecu::platform {

class MessageTranslator {
public:
    static ports::CanFrame someipToCanFrame(
        uint32_t can_id, uint8_t dlc,
        const std::vector<uint8_t>& someip_payload);

    static std::vector<uint8_t> canFrameToSomeipPayload(
        const ports::CanFrame& frame);
};

}  // namespace body_ecu::platform
