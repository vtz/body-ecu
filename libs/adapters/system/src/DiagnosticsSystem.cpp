#include "DiagnosticsSystem.h"

namespace body_ecu::adapters {

DiagnosticsSystem::DiagnosticsSystem() : handler_(dtc_store_) {}

void DiagnosticsSystem::addTransport(platform::ITransportLayer* transport) {
    transports_.push_back(transport);
}

void DiagnosticsSystem::addProvider(ports::IDiagDataProvider* provider) {
    handler_.addProvider(provider);
}

void DiagnosticsSystem::init() {
    for (auto* transport : transports_) {
        transport->setRequestHandler(
            [this](const platform::DiagRequest& req) {
                return handler_.handleRequest(req);
            });
    }
}

void DiagnosticsSystem::run() {
    // Transports are event-driven (DoIP: TCP accept loop, DoCAN: CAN rx).
    // In the full OpenBSW build, DoIpServerSystem and DoCanSystem run
    // as separate AsyncLifecycleComponents managed by LifecycleManager.
}

void DiagnosticsSystem::shutdown() {
    // Transports shut down independently via their own lifecycle.
}

}  // namespace body_ecu::adapters
