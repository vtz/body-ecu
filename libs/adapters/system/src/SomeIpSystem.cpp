#include "SomeIpSystem.h"

#include <cstdio>

namespace body_ecu::adapters {

SomeIpSystem::SomeIpSystem(const SomeIpConfig& config) : config_(config) {}

SomeIpSystem::~SomeIpSystem() {
    if (running_) {
        shutdown();
    }
}

void SomeIpSystem::init() {
#ifdef HAS_OPENSOMEIP
    is_server_ = (config_.role == SomeIpRole::Server);
    if (is_server_) {
        someip::transport::Endpoint local(config_.host, config_.port);
        transport_ = std::make_shared<someip::transport::UdpTransport>(local);
    } else {
        someip::transport::Endpoint local("0.0.0.0", 0);
        transport_ = std::make_shared<someip::transport::UdpTransport>(local);
        server_endpoint_ = someip::transport::Endpoint(config_.host, config_.port);
    }
    transport_->set_listener(this);
    std::printf("[SOME/IP] Initialized (%s) %s:%u\n",
                is_server_ ? "server" : "client -> ",
                config_.host.c_str(), config_.port);
#endif
}

void SomeIpSystem::run() {
    running_ = true;
#ifdef HAS_OPENSOMEIP
    auto result = transport_->start();
    if (result != someip::Result::SUCCESS) {
        std::printf("[SOME/IP] Failed to start transport (error %d)\n",
                    static_cast<int>(result));
        return;
    }
    std::printf("[SOME/IP] Transport running on %s\n",
                transport_->get_local_endpoint().to_string().c_str());
#endif
}

void SomeIpSystem::shutdown() {
    running_ = false;
#ifdef HAS_OPENSOMEIP
    if (transport_) {
        transport_->stop();
        std::printf("[SOME/IP] Transport stopped\n");
    }
#endif
    PlatformLockGuard lock(mutex_);
    methods_.clear();
    events_.clear();
}

void SomeIpSystem::registerMethod(uint16_t service_id, uint16_t method_id,
                                  ports::MethodHandler handler) {
    PlatformLockGuard lock(mutex_);
    methods_[makeKey(service_id, method_id)] = std::move(handler);
}

void SomeIpSystem::registerEvent(uint16_t service_id, uint16_t event_id,
                                 uint16_t eventgroup_id) {
    PlatformLockGuard lock(mutex_);
    events_.push_back({service_id, event_id, eventgroup_id});
}

void SomeIpSystem::sendEvent(uint16_t service_id, uint16_t event_id,
                             const std::vector<uint8_t>& payload) {
    ports::SomeIpMessage msg;
    msg.service_id = service_id;
    msg.method_id = event_id;
    msg.message_type = 0x02;  // Notification
    msg.return_code = 0x00;
    msg.payload = payload;
    sent_events_.push_back(msg);

#ifdef HAS_OPENSOMEIP
    if (transport_ && running_) {
        auto someip_msg = toSomeIp(msg);
        PlatformLockGuard lock(mutex_);
        for (const auto& client : known_clients_) {
            (void)transport_->send_message(someip_msg, client);
        }
    }
#endif
}

void SomeIpSystem::sendResponse(const ports::SomeIpMessage& response) {
    sent_responses_.push_back(response);

#ifdef HAS_OPENSOMEIP
    if (transport_ && running_) {
        auto someip_msg = toSomeIp(response);
        if (is_server_) {
            PlatformLockGuard lock(mutex_);
            for (const auto& client : known_clients_) {
                (void)transport_->send_message(someip_msg, client);
            }
        } else {
            (void)transport_->send_message(someip_msg, server_endpoint_);
        }
    }
#endif
}

ports::SomeIpMessage SomeIpSystem::dispatch(
    const ports::SomeIpMessage& request) {
    ports::MethodHandler handler;
    {
        PlatformLockGuard lock(mutex_);
        auto it = methods_.find(makeKey(request.service_id, request.method_id));
        if (it == methods_.end()) {
            ports::SomeIpMessage err = request;
            err.message_type = 0x81;  // Error
            err.return_code = 0x05;   // E_NOT_OK
            return err;
        }
        handler = it->second;
    }
    return handler(request);
}

// --- opensomeip transport integration ---

#ifdef HAS_OPENSOMEIP

void SomeIpSystem::on_message_received(
    someip::MessagePtr message,
    const someip::transport::Endpoint& sender) {
    if (!message) return;

    {
        PlatformLockGuard lock(mutex_);
        known_clients_.insert(sender);
    }

    auto incoming = fromSomeIp(*message);

    std::printf("[SOME/IP] Received service=0x%04X method=0x%04X type=0x%02X from %s payload=[",
                incoming.service_id, incoming.method_id,
                incoming.message_type, sender.to_string().c_str());
    for (size_t i = 0; i < incoming.payload.size(); ++i) {
        std::printf("%s0x%02X", i ? " " : "", incoming.payload[i]);
    }
    std::printf("]\n");

    if (message->is_request()) {
        auto response = dispatch(incoming);
        auto resp_msg = toSomeIp(response);
        (void)transport_->send_message(resp_msg, sender);
    } else {
        ports::MethodHandler handler;
        {
            PlatformLockGuard lock(mutex_);
            auto key = makeKey(incoming.service_id, incoming.method_id);
            auto it = methods_.find(key);
            if (it != methods_.end()) {
                handler = it->second;
            }
        }
        if (handler) {
            handler(incoming);
        }
    }
}

void SomeIpSystem::on_connection_established(
    const someip::transport::Endpoint& endpoint) {
    std::printf("[SOME/IP] Connection established: %s\n",
                endpoint.to_string().c_str());
}

void SomeIpSystem::on_connection_lost(
    const someip::transport::Endpoint& endpoint) {
    std::printf("[SOME/IP] Connection lost: %s\n",
                endpoint.to_string().c_str());
    PlatformLockGuard lock(mutex_);
    known_clients_.erase(endpoint);
}

void SomeIpSystem::on_error(someip::Result error) {
    std::printf("[SOME/IP] Transport error: %d\n", static_cast<int>(error));
}

ports::SomeIpMessage SomeIpSystem::fromSomeIp(const someip::Message& msg) {
    ports::SomeIpMessage result;
    result.service_id = msg.get_service_id();
    result.method_id = msg.get_method_id();
    result.client_id = msg.get_client_id();
    result.session_id = msg.get_session_id();
    result.message_type = static_cast<uint8_t>(msg.get_message_type());
    result.return_code = static_cast<uint8_t>(msg.get_return_code());
    result.payload = msg.get_payload();
    return result;
}

someip::Message SomeIpSystem::toSomeIp(const ports::SomeIpMessage& msg) {
    someip::MessageId msg_id(msg.service_id, msg.method_id);
    someip::RequestId req_id(msg.client_id, msg.session_id);
    auto type = static_cast<someip::MessageType>(msg.message_type);
    auto rc = static_cast<someip::ReturnCode>(msg.return_code);
    someip::Message result(msg_id, req_id, type, rc);
    result.set_payload(msg.payload);
    return result;
}

#endif  // HAS_OPENSOMEIP

}  // namespace body_ecu::adapters
