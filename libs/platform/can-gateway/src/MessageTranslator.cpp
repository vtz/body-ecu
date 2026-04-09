#include "can_gateway/MessageTranslator.h"

#include <algorithm>

namespace body_ecu::platform {

ports::CanFrame MessageTranslator::someipToCanFrame(
    uint32_t can_id, uint8_t dlc,
    const std::vector<uint8_t>& someip_payload) {
    ports::CanFrame frame;
    frame.id = can_id;
    frame.dlc = dlc;
    auto copy_len = std::min(static_cast<size_t>(dlc), someip_payload.size());
    copy_len = std::min(copy_len, sizeof(frame.data));
    std::copy_n(someip_payload.begin(), copy_len, frame.data);
    return frame;
}

std::vector<uint8_t> MessageTranslator::canFrameToSomeipPayload(
    const ports::CanFrame& frame) {
    auto len = std::min(static_cast<size_t>(frame.dlc), sizeof(frame.data));
    return {frame.data, frame.data + len};
}

}  // namespace body_ecu::platform
