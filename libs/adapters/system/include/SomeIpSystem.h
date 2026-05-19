#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#else
#include <mutex>
#include <thread>
#endif

#include <lifecycle/SimpleLifecycleComponent.h>
#include "ports/ISomeIpService.h"

#ifdef HAS_OPENSOMEIP
#include "someip/message.h"
#include "transport/udp_transport.h"
#include "transport/transport.h"
#include "sd/sd_server.h"
#include "sd/sd_client.h"
#include "sd/sd_types.h"
#endif

namespace body_ecu::adapters {

#ifdef __ZEPHYR__
// Zephyr MCU: cooperative scheduling — no preemptive data races possible
// within the SOME/IP dispatch path. Use no-op locks to avoid pulling in
// k_mutex (which has native_sim linker issues) while keeping the POSIX
// HPC path properly protected.
class PlatformMutex {
public:
    void lock() {}
    void unlock() {}
};
class PlatformLockGuard {
public:
    explicit PlatformLockGuard(PlatformMutex&) {}
    PlatformLockGuard(const PlatformLockGuard&) = delete;
    PlatformLockGuard& operator=(const PlatformLockGuard&) = delete;
};
#else
using PlatformMutex = std::mutex;
template <class M>
using PlatformLockGuard = std::lock_guard<M>;
#endif

enum class SomeIpRole { Server, Client };

struct SomeIpConfig {
    std::string host{"0.0.0.0"};
    uint16_t port{30490};
    SomeIpRole role{SomeIpRole::Server};
    bool enable_sd{true};
    std::string sd_multicast{"239.255.255.251"};
    uint16_t sd_port{30491};
    uint32_t sd_offer_interval_ms{5000};
};

class SomeIpSystem : public lifecycle::SimpleLifecycleComponent
                   , public ports::ISomeIpService
#ifdef HAS_OPENSOMEIP
                   , public ::someip::transport::ITransportListener
#endif
{
public:
    explicit SomeIpSystem(const SomeIpConfig& config = {});
    ~SomeIpSystem();

    void init() override;
    void run() override;
    void shutdown() override;

    bool isRunning() const { return running_; }

    // ISomeIpService
    void registerMethod(uint16_t service_id, uint16_t method_id,
                        ports::MethodHandler handler) override;
    void registerEvent(uint16_t service_id, uint16_t event_id,
                       uint16_t eventgroup_id) override;
    void sendEvent(uint16_t service_id, uint16_t event_id,
                   const std::vector<uint8_t>& payload) override;
    void sendResponse(const ports::SomeIpMessage& response) override;

    ports::SomeIpMessage dispatch(const ports::SomeIpMessage& request);

    const std::vector<ports::SomeIpMessage>& sentEvents() const {
        return sent_events_;
    }

    const std::vector<ports::SomeIpMessage>& sentResponses() const {
        return sent_responses_;
    }

#ifdef HAS_OPENSOMEIP
    // ITransportListener
    void on_message_received(::someip::MessagePtr message,
                             const ::someip::transport::Endpoint& sender) override;
    void on_connection_lost(const ::someip::transport::Endpoint& endpoint) override;
    void on_connection_established(const ::someip::transport::Endpoint& endpoint) override;
    void on_error(::someip::Result error) override;
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
    bool running_{false};
    bool dispatching_{false};
    PlatformMutex mutex_;
    std::map<MethodKey, ports::MethodHandler> methods_;
    std::vector<EventRegistration> events_;
    std::vector<ports::SomeIpMessage> sent_events_;
    std::vector<ports::SomeIpMessage> sent_responses_;
    std::vector<ports::SomeIpMessage> pending_events_;

#ifdef HAS_OPENSOMEIP
    std::shared_ptr<::someip::transport::UdpTransport> transport_;
    std::set<::someip::transport::Endpoint> known_clients_;
    ::someip::transport::Endpoint server_endpoint_;
    bool is_server_{false};

    std::unique_ptr<::someip::sd::SdServer> sd_server_;
    std::unique_ptr<::someip::sd::SdClient> sd_client_;

    void initSd();
    void shutdownSd();
    void onServiceFound(const std::vector<::someip::sd::ServiceInstance>& services);

    static ports::SomeIpMessage fromSomeIp(const ::someip::Message& msg);
    static ::someip::Message toSomeIp(const ports::SomeIpMessage& msg);
#endif
};

}  // namespace body_ecu::adapters
