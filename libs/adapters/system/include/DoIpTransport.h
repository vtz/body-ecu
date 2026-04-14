#pragma once

#include <cstdint>
#include <vector>

#include "diagnostics/ITransportLayer.h"
#include "lifecycle/ILifecycleComponent.h"

namespace body_ecu::adapters {

class DoIpTransport : public lifecycle::ILifecycleComponent
                    , public platform::ITransportLayer {
public:
    static constexpr uint16_t kDoIpPort = 13400;
    static constexpr uint16_t kLogicalAddress = 0x0E80;

    void setRequestHandler(platform::DiagRequestHandler handler) override;
    void sendResponse(const platform::DiagResponse& response) override;

    void onDoIpRequest(const std::vector<uint8_t>& data);

    bool isConnected() const { return connected_; }

    void init() override;
    void run() override {}
    void shutdown() override;

private:
    platform::DiagRequestHandler handler_;
    bool connected_{false};
};

}  // namespace body_ecu::adapters
