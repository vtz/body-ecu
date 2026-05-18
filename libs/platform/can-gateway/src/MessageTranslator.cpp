#include "can_gateway/MessageTranslator.h"

#include <algorithm>

namespace body_ecu::platform {

ports::CanFrame MessageTranslator::someipToCanFrame(
    uint32_t can_id, uint8_t dlc,
    const std::vector<uint8_t>& someip_payload) {
    ports::CanFrame frame;
    frame.id = can_id;
    frame.dlc = std::min(dlc, static_cast<uint8_t>(sizeof(frame.data)));
    auto copy_len = std::min(static_cast<size_t>(frame.dlc), someip_payload.size());
    std::copy_n(someip_payload.begin(), copy_len, frame.data);
    return frame;
}

std::vector<uint8_t> MessageTranslator::canFrameToSomeipPayload(
    const ports::CanFrame& frame) {
    auto len = std::min(static_cast<size_t>(frame.dlc), sizeof(frame.data));
    return {frame.data, frame.data + len};
}

}  // namespace body_ecu::platform
