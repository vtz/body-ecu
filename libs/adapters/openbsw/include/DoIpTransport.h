#pragma once

#include <cstdint>
#include <vector>

#include "diagnostics/ITransportLayer.h"

namespace body_ecu::adapters {

/// DoIP transport layer wrapping OpenBSW's DoIpServerSystem.
/// Handles TCP connections for diagnostic requests over Ethernet.
/// In the full build, this registers with OpenBSW's DoIpServerSystem
/// which manages up to 5 concurrent TCP connections, vehicle identification
/// via UDP, and routing activation.
class DoIpTransport : public platform::ITransportLayer {
public:
    static constexpr uint16_t kDoIpPort = 13400;
    static constexpr uint16_t kLogicalAddress = 0x0E80;

    void setRequestHandler(platform::DiagRequestHandler handler) override;
    void sendResponse(const platform::DiagResponse& response) override;

    /// Called by the OpenBSW DoIpServerSystem when a diagnostic message
    /// arrives over TCP after routing activation.
    void onDoIpRequest(const std::vector<uint8_t>& data);

    bool isConnected() const { return connected_; }

    void init();
    void shutdown();

private:
    platform::DiagRequestHandler handler_;
    bool connected_{false};
};

}  // namespace body_ecu::adapters
