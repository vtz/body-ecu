#pragma once

#include <cstdint>
#include <vector>

#include "diagnostics/ITransportLayer.h"
#include "lifecycle/ILifecycleComponent.h"
#include "ports/ICanBus.h"

namespace body_ecu::adapters {

class DoCanTransport : public lifecycle::ILifecycleComponent
                     , public platform::ITransportLayer {
public:
    static constexpr uint32_t kDiagRxCanId = 0x600;
    static constexpr uint32_t kDiagTxCanId = 0x601;

    explicit DoCanTransport(ports::ICanBus& can);

    void setRequestHandler(platform::DiagRequestHandler handler) override;
    void sendResponse(const platform::DiagResponse& response) override;

    void init() override;
    void run() override {}
    void shutdown() override;

private:
    void onCanFrame(const ports::CanFrame& frame);

    ports::ICanBus& can_;
    platform::DiagRequestHandler handler_;
};

}  // namespace body_ecu::adapters
