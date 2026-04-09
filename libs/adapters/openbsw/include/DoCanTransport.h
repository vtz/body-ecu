#pragma once

#include <cstdint>
#include <vector>

#include "diagnostics/ITransportLayer.h"
#include "ports/ICanBus.h"

namespace body_ecu::adapters {

/// DoCAN transport layer wrapping OpenBSW's DoCanSystem.
/// Handles UDS diagnostic requests over CAN-FD using ISO-TP segmentation.
/// In the full build, this uses OpenBSW's cpp2can CAN stack with
/// ISO 15765-2 transport protocol for multi-frame diagnostics.
class DoCanTransport : public platform::ITransportLayer {
public:
    static constexpr uint32_t kDiagRxCanId = 0x600;
    static constexpr uint32_t kDiagTxCanId = 0x601;

    explicit DoCanTransport(ports::ICanBus& can);

    void setRequestHandler(platform::DiagRequestHandler handler) override;
    void sendResponse(const platform::DiagResponse& response) override;

    void init();
    void shutdown();

private:
    void onCanFrame(const ports::CanFrame& frame);

    ports::ICanBus& can_;
    platform::DiagRequestHandler handler_;
};

}  // namespace body_ecu::adapters
