#include "autosd_adapters/NatsCloudTransportAdapter.h"

#include <cstdio>

#ifdef HAS_NATS

#include <nats/nats.h>

namespace body_ecu::adapters {

struct NatsCloudTransportAdapter::Impl {
    natsConnection* conn{nullptr};
    std::vector<natsSubscription*> subs;
};

static void onMessage(natsConnection* /*nc*/, natsSubscription* /*sub*/,
                       natsMsg* msg, void* closure) {
    auto* adapter = static_cast<NatsCloudTransportAdapter*>(closure);

    const char* subject = natsMsg_GetSubject(msg);
    const char* data = natsMsg_GetData(msg);
    int len = natsMsg_GetDataLength(msg);

    std::vector<uint8_t> payload(
        reinterpret_cast<const uint8_t*>(data),
        reinterpret_cast<const uint8_t*>(data) + len);

    adapter->dispatchMessage(std::string(subject), payload);

    natsMsg_Destroy(msg);
}

NatsCloudTransportAdapter::NatsCloudTransportAdapter(const NatsConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {}

NatsCloudTransportAdapter::~NatsCloudTransportAdapter() {
    disconnect();
}

bool NatsCloudTransportAdapter::connect() {
    std::printf("[NATS] Connecting to %s\n", config_.url.c_str());

    natsStatus s = natsConnection_ConnectTo(&impl_->conn, config_.url.c_str());
    if (s != NATS_OK) {
        std::printf("[NATS] Connection failed: %s\n", natsStatus_GetText(s));
        return false;
    }

    connected_ = true;
    std::printf("[NATS] Connected\n");
    return true;
}

void NatsCloudTransportAdapter::disconnect() {
    if (!connected_) return;

    for (auto* sub : impl_->subs) {
        natsSubscription_Unsubscribe(sub);
        natsSubscription_Destroy(sub);
    }
    impl_->subs.clear();

    if (impl_->conn) {
        natsConnection_Close(impl_->conn);
        natsConnection_Destroy(impl_->conn);
        impl_->conn = nullptr;
    }

    connected_ = false;
    std::printf("[NATS] Disconnected\n");
}

bool NatsCloudTransportAdapter::publish(const std::string& subject,
                                        const std::vector<uint8_t>& data) {
    if (!connected_ || !impl_->conn) return false;

    natsStatus s = natsConnection_Publish(
        impl_->conn, subject.c_str(),
        reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()));

    if (s != NATS_OK) {
        std::printf("[NATS] Publish %s failed: %s\n", subject.c_str(),
                    natsStatus_GetText(s));
        return false;
    }

    std::printf("[NATS] PUBLISH %s (%zu bytes)\n", subject.c_str(),
                data.size());
    return true;
}

void NatsCloudTransportAdapter::subscribe(
    const std::string& subject, ports::CloudMessageCallback callback) {
    if (!connected_ || !impl_->conn) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_[subject] = callback;
    }

    natsSubscription* sub = nullptr;
    natsStatus s = natsConnection_Subscribe(
        &sub, impl_->conn, subject.c_str(), onMessage, this);

    if (s != NATS_OK) {
        std::printf("[NATS] Subscribe %s failed: %s\n", subject.c_str(),
                    natsStatus_GetText(s));
        return;
    }

    impl_->subs.push_back(sub);
    std::printf("[NATS] SUBSCRIBE %s\n", subject.c_str());
}

void NatsCloudTransportAdapter::dispatchMessage(
    const std::string& subject, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscribers_.find(subject);
    if (it != subscribers_.end()) {
        it->second(subject, data);
    }
}

}  // namespace body_ecu::adapters

#else  // !HAS_NATS -- stub implementation for development builds

namespace body_ecu::adapters {

struct NatsCloudTransportAdapter::Impl {};

NatsCloudTransportAdapter::NatsCloudTransportAdapter(const NatsConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {}

NatsCloudTransportAdapter::~NatsCloudTransportAdapter() = default;

bool NatsCloudTransportAdapter::connect() {
    std::printf("[NATS] Connecting to %s (stub)\n", config_.url.c_str());
    connected_ = true;
    return true;
}

void NatsCloudTransportAdapter::disconnect() {
    connected_ = false;
}

bool NatsCloudTransportAdapter::publish(const std::string& subject,
                                        const std::vector<uint8_t>& data) {
    if (!connected_) return false;
    std::printf("[NATS] PUBLISH %s (%zu bytes) (stub)\n", subject.c_str(),
                data.size());
    return true;
}

void NatsCloudTransportAdapter::subscribe(
    const std::string& subject, ports::CloudMessageCallback callback) {
    if (!connected_) return;
    std::printf("[NATS] SUBSCRIBE %s (stub)\n", subject.c_str());

    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_[subject] = callback;
}

void NatsCloudTransportAdapter::dispatchMessage(
    const std::string& subject, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscribers_.find(subject);
    if (it != subscribers_.end()) {
        it->second(subject, data);
    }
}

}  // namespace body_ecu::adapters

#endif  // HAS_NATS
