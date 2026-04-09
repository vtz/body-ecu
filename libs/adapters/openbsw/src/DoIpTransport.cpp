#include "DoIpTransport.h"

namespace body_ecu::adapters {

void DoIpTransport::setRequestHandler(platform::DiagRequestHandler handler) {
    handler_ = std::move(handler);
}

void DoIpTransport::sendResponse(const platform::DiagResponse& response) {
    // In the full build, this serializes the UDS response into a DoIP
    // diagnostic message and sends it over the active TCP connection
    // using OpenBSW's DoIpServerSystem transport.
    (void)response;
}

void DoIpTransport::onDoIpRequest(const std::vector<uint8_t>& data) {
    if (!handler_) return;
    auto response = handler_(data);
    sendResponse(response);
}

void DoIpTransport::init() {
    connected_ = true;
    // In the full build, this initializes the TCP listener on kDoIpPort
    // and registers this transport with OpenBSW's LifecycleManager.
}

void DoIpTransport::shutdown() {
    connected_ = false;
    handler_ = nullptr;
}

}  // namespace body_ecu::adapters
