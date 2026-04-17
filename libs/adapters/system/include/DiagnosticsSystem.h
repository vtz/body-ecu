#pragma once

#include "diagnostics/DtcStore.h"
#include "diagnostics/ITransportLayer.h"
#include "diagnostics/UdsServiceHandler.h"
#include <lifecycle/SimpleLifecycleComponent.h>
#include "ports/IDiagDataProvider.h"

namespace body_ecu::adapters {

class DiagnosticsSystem : public lifecycle::SimpleLifecycleComponent {
public:
    DiagnosticsSystem();

    void addTransport(platform::ITransportLayer* transport);
    void addProvider(ports::IDiagDataProvider* provider);

    void init() override;
    void run() override;
    void shutdown() override;

    platform::UdsServiceHandler& handler() { return handler_; }
    platform::DtcStore& dtcStore() { return dtc_store_; }

private:
    platform::DtcStore dtc_store_;
    platform::UdsServiceHandler handler_;
    std::vector<platform::ITransportLayer*> transports_;
};

}  // namespace body_ecu::adapters
