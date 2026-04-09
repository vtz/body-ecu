#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "ports/ISomeIpService.h"

namespace body_ecu::adapters {

enum class LifecycleState { Created, Initialized, Running, Shutdown };

struct SomeIpConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{30490};
};

/// SomeIpSystem implements ISomeIpService and manages the SOME/IP
/// transport lifecycle. In the full Zephyr build this wraps OpenSOME/IP;
/// the method/event dispatch logic is testable standalone.
class SomeIpSystem : public ports::ISomeIpService {
public:
    explicit SomeIpSystem(const SomeIpConfig& config = {});

    void init();
    void run();
    void shutdown();

    LifecycleState state() const { return state_; }

    // ISomeIpService
    void registerMethod(uint16_t service_id, uint16_t method_id,
                        ports::MethodHandler handler) override;
    void registerEvent(uint16_t service_id, uint16_t event_id,
                       uint16_t eventgroup_id) override;
    void sendEvent(uint16_t service_id, uint16_t event_id,
                   const std::vector<uint8_t>& payload) override;
    void sendResponse(const ports::SomeIpMessage& response) override;

    /// Simulate an incoming request (for testing dispatch logic).
    ports::SomeIpMessage dispatch(const ports::SomeIpMessage& request);

    /// Access sent events (for test verification).
    const std::vector<ports::SomeIpMessage>& sentEvents() const {
        return sent_events_;
    }

    /// Access sent responses (for test verification).
    const std::vector<ports::SomeIpMessage>& sentResponses() const {
        return sent_responses_;
    }

private:
    using MethodKey = uint32_t;
    static MethodKey makeKey(uint16_t service_id, uint16_t method_id) {
        return (static_cast<uint32_t>(service_id) << 16) | method_id;
    }

    struct EventRegistration {
        uint16_t service_id;
        uint16_t event_id;
        uint16_t eventgroup_id;
    };

    SomeIpConfig config_;
    LifecycleState state_{LifecycleState::Created};
    std::map<MethodKey, ports::MethodHandler> methods_;
    std::vector<EventRegistration> events_;
    std::vector<ports::SomeIpMessage> sent_events_;
    std::vector<ports::SomeIpMessage> sent_responses_;
};

}  // namespace body_ecu::adapters
