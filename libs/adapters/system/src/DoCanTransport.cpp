#include "DoCanTransport.h"

#include <cstring>

namespace body_ecu::adapters {

DoCanTransport::DoCanTransport(ports::ICanBus& can) : can_(can) {}

void DoCanTransport::setRequestHandler(platform::DiagRequestHandler handler) {
    handler_ = std::move(handler);
}

void DoCanTransport::sendResponse(const platform::DiagResponse& response) {
    // Single-frame response for payloads <= 7 bytes (SF_DL in byte 0).
    // Multi-frame (First Frame + Consecutive Frames) for longer payloads
    // would use OpenBSW's ISO-TP implementation.
    ports::CanFrame frame;
    frame.id = kDiagTxCanId;

    if (response.size() <= 7) {
        frame.dlc = static_cast<uint8_t>(response.size() + 1);
        frame.data[0] = static_cast<uint8_t>(response.size());  // SF PCI
        std::memcpy(&frame.data[1], response.data(), response.size());
        can_.send(frame);
    }
    // Multi-frame responses delegated to OpenBSW's ISO-TP in full build.
}

void DoCanTransport::init() {
    can_.addRxCallback([this](const ports::CanFrame& f) { onCanFrame(f); });
    transitionDone();
}

void DoCanTransport::shutdown() {
    handler_ = nullptr;
    transitionDone();
}

void DoCanTransport::onCanFrame(const ports::CanFrame& frame) {
    if (frame.id != kDiagRxCanId) return;
    if (!handler_) return;

    // Single-frame: PCI byte 0 contains length
    uint8_t sf_dl = frame.data[0] & 0x0F;
    if (sf_dl == 0 || sf_dl > 7) return;

    platform::DiagRequest request(frame.data + 1, frame.data + 1 + sf_dl);
    auto response = handler_(request);
    sendResponse(response);
}

}  // namespace body_ecu::adapters
