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
    transitionDone();
}

void DiagnosticsSystem::run() {
    transitionDone();
}

void DiagnosticsSystem::shutdown() {
    transitionDone();
}

}  // namespace body_ecu::adapters
