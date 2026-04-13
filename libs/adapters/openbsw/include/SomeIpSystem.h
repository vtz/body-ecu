#pragma once

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ports/ISomeIpService.h"

#ifdef HAS_OPENSOMEIP
#include "someip/message.h"
#include "transport/udp_transport.h"
#include "transport/transport.h"
#endif

namespace body_ecu::adapters {

enum class LifecycleState { Created, Initialized, Running, Shutdown };

enum class SomeIpRole { Server, Client };

struct SomeIpConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{30490};
    SomeIpRole role{SomeIpRole::Server};
};

/// SomeIpSystem implements ISomeIpService and manages the SOME/IP
/// transport lifecycle. When linked with opensomeip (HAS_OPENSOMEIP),
/// uses real UDP transport. Otherwise falls back to in-memory dispatch
/// for unit testing.
class SomeIpSystem : public ports::ISomeIpService
#ifdef HAS_OPENSOMEIP
    , public someip::transport::ITransportListener
#endif
{
public:
    explicit SomeIpSystem(const SomeIpConfig& config = {});
    ~SomeIpSystem();

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

#ifdef HAS_OPENSOMEIP
    // ITransportListener
    void on_message_received(someip::MessagePtr message,
                             const someip::transport::Endpoint& sender) override;
    void on_connection_lost(const someip::transport::Endpoint& endpoint) override;
    void on_connection_established(const someip::transport::Endpoint& endpoint) override;
    void on_error(someip::Result error) override;
#endif

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
    std::mutex mutex_;
    std::map<MethodKey, ports::MethodHandler> methods_;
    std::vector<EventRegistration> events_;
    std::vector<ports::SomeIpMessage> sent_events_;
    std::vector<ports::SomeIpMessage> sent_responses_;

#ifdef HAS_OPENSOMEIP
    std::shared_ptr<someip::transport::UdpTransport> transport_;
    std::set<someip::transport::Endpoint> known_clients_;
    someip::transport::Endpoint server_endpoint_;
    bool is_server_{false};

    static ports::SomeIpMessage fromSomeIp(const someip::Message& msg);
    static someip::Message toSomeIp(const ports::SomeIpMessage& msg);
#endif
};

}  // namespace body_ecu::adapters
