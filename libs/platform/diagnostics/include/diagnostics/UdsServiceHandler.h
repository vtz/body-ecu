#pragma once

#include <cstdint>
#include <vector>

#include "diagnostics/DtcStore.h"
#include "diagnostics/ITransportLayer.h"
#include "ports/IDiagDataProvider.h"

namespace body_ecu::platform {

enum class DiagSession : uint8_t {
    Default = 0x01,
    Extended = 0x03,
};

/// UDS service handler implementing 0x10, 0x22, 0x2F, 0x19.
class UdsServiceHandler {
public:
    UdsServiceHandler(DtcStore& dtc_store);

    void addProvider(ports::IDiagDataProvider* provider);

    DiagResponse handleRequest(const DiagRequest& request);

    DiagSession currentSession() const { return session_; }

private:
    DiagResponse handleDiagSessionControl(const DiagRequest& request);
    DiagResponse handleReadDataById(const DiagRequest& request);
    DiagResponse handleIoControl(const DiagRequest& request);
    DiagResponse handleReadDtc(const DiagRequest& request);

    DiagResponse negativeResponse(uint8_t sid, uint8_t nrc);

    DtcStore& dtc_store_;
    std::vector<ports::IDiagDataProvider*> providers_;
    DiagSession session_{DiagSession::Default};
};

}  // namespace body_ecu::platform
