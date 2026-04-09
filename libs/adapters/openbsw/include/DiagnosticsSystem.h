#pragma once

#include "diagnostics/DtcStore.h"
#include "diagnostics/ITransportLayer.h"
#include "diagnostics/UdsServiceHandler.h"
#include "ports/IDiagDataProvider.h"

namespace body_ecu::adapters {

/// AsyncLifecycleComponent wrapper that owns the UDS diagnostics stack.
/// In the full build, this wires DoIpServerSystem (Ethernet) and
/// DoCanSystem (CAN-FD) as transport layers for dual-transport UDS.
class DiagnosticsSystem {
public:
    DiagnosticsSystem();

    void addTransport(platform::ITransportLayer* transport);
    void addProvider(ports::IDiagDataProvider* provider);

    void init();
    void run();
    void shutdown();

    platform::UdsServiceHandler& handler() { return handler_; }
    platform::DtcStore& dtcStore() { return dtc_store_; }

private:
    platform::DtcStore dtc_store_;
    platform::UdsServiceHandler handler_;
    std::vector<platform::ITransportLayer*> transports_;
};

}  // namespace body_ecu::adapters
