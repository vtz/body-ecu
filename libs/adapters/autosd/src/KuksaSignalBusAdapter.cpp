#include "autosd_adapters/KuksaSignalBusAdapter.h"

#include <cstdio>

#ifdef HAS_KUKSA_GRPC

#include <atomic>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "kuksa/val/v2/val.grpc.pb.h"
#include "kuksa/val/v2/val.pb.h"
#include "kuksa/val/v2/types.pb.h"

namespace body_ecu::adapters {

namespace {

kuksa::val::v2::Value toProtoValue(const ports::SignalValue& sv) {
    kuksa::val::v2::Value v;
    std::visit([&v](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>)
            v.set_bool_(arg);
        else if constexpr (std::is_same_v<T, int32_t>)
            v.set_int32(arg);
        else if constexpr (std::is_same_v<T, float>)
            v.set_float_(arg);
        else if constexpr (std::is_same_v<T, std::string>)
            v.set_string(arg);
    }, sv);
    return v;
}

ports::SignalValue fromProtoValue(const kuksa::val::v2::Value& v) {
    switch (v.typed_value_case()) {
        case kuksa::val::v2::Value::kBool:
            return ports::SignalValue{v.bool_()};
        case kuksa::val::v2::Value::kInt32:
            return ports::SignalValue{static_cast<int32_t>(v.int32())};
        case kuksa::val::v2::Value::kFloat:
            return ports::SignalValue{v.float_()};
        case kuksa::val::v2::Value::kString:
            return ports::SignalValue{v.string()};
        default:
            return ports::SignalValue{false};
    }
}

}  // namespace

struct KuksaSignalBusAdapter::Impl {
    std::shared_ptr<grpc::Channel> channel;
    std::unique_ptr<kuksa::val::v2::VAL::Stub> stub;
    std::atomic<bool> streaming{false};
    std::vector<std::thread> stream_threads;
    std::mutex ctx_mutex;
    std::vector<std::shared_ptr<grpc::ClientContext>> stream_contexts;
};

KuksaSignalBusAdapter::KuksaSignalBusAdapter(const KuksaConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {}

KuksaSignalBusAdapter::~KuksaSignalBusAdapter() {
    disconnect();
}

void KuksaSignalBusAdapter::connect() {
    std::string target = config_.host + ":" + std::to_string(config_.port);
    std::printf("[Kuksa] Connecting to %s\n", target.c_str());

    impl_->channel = grpc::CreateChannel(target,
                                         grpc::InsecureChannelCredentials());
    impl_->stub = kuksa::val::v2::VAL::NewStub(impl_->channel);
    impl_->streaming = true;
    connected_ = true;

    std::printf("[Kuksa] Connected\n");
}

void KuksaSignalBusAdapter::disconnect() {
    if (!connected_) return;

    impl_->streaming = false;
    {
        std::lock_guard<std::mutex> lock(impl_->ctx_mutex);
        for (auto& ctx : impl_->stream_contexts) {
            ctx->TryCancel();
        }
    }
    for (auto& t : impl_->stream_threads) {
        if (t.joinable()) t.join();
    }
    impl_->stream_threads.clear();
    {
        std::lock_guard<std::mutex> lock(impl_->ctx_mutex);
        impl_->stream_contexts.clear();
    }
    impl_->stub.reset();
    impl_->channel.reset();
    connected_ = false;
    std::printf("[Kuksa] Disconnected\n");
}

bool KuksaSignalBusAdapter::publish(const std::string& path,
                                    const ports::SignalValue& value) {
    if (!connected_ || !impl_->stub) return false;

    kuksa::val::v2::PublishValueRequest request;
    request.mutable_signal_id()->set_path(path);

    auto* dp = request.mutable_data_point();
    *dp->mutable_value() = toProtoValue(value);
    auto* ts = dp->mutable_timestamp();
    auto now = std::chrono::system_clock::now();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch());
    ts->set_seconds(secs.count());

    grpc::ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() +
                     std::chrono::seconds(5));

    kuksa::val::v2::PublishValueResponse response;
    auto status = impl_->stub->PublishValue(&ctx, request, &response);

    if (!status.ok()) {
        std::printf("[Kuksa] PublishValue %s failed: %s\n", path.c_str(),
                    status.error_message().c_str());
        return false;
    }

    std::printf("[Kuksa] SET %s OK\n", path.c_str());
    return true;
}

void KuksaSignalBusAdapter::subscribe(const std::string& path,
                                      ports::SignalCallback callback) {
    if (!connected_ || !impl_->stub) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_[path] = callback;
    }

    std::printf("[Kuksa] SUBSCRIBE %s\n", path.c_str());

    auto ctx = std::make_shared<grpc::ClientContext>();
    {
        std::lock_guard<std::mutex> lock(impl_->ctx_mutex);
        impl_->stream_contexts.push_back(ctx);
    }

    impl_->stream_threads.emplace_back([this, path, ctx]() {
        kuksa::val::v2::SubscribeRequest request;
        request.add_signal_paths(path);

        auto reader = impl_->stub->Subscribe(ctx.get(), request);

        kuksa::val::v2::SubscribeResponse response;
        while (impl_->streaming && reader->Read(&response)) {
            for (const auto& [sig_path, dp] : response.entries()) {
                if (!dp.has_value()) continue;

                auto sv = fromProtoValue(dp.value());
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = subscribers_.find(sig_path);
                if (it != subscribers_.end()) {
                    it->second(sig_path, sv);
                }
            }
        }
    });
}

std::optional<ports::SignalValue> KuksaSignalBusAdapter::get(
    const std::string& path) const {
    if (!connected_ || !impl_->stub) return std::nullopt;

    kuksa::val::v2::GetValueRequest request;
    request.mutable_signal_id()->set_path(path);

    grpc::ClientContext ctx;
    ctx.set_deadline(std::chrono::system_clock::now() +
                     std::chrono::seconds(5));

    kuksa::val::v2::GetValueResponse response;
    auto status = impl_->stub->GetValue(&ctx, request, &response);

    if (!status.ok()) {
        std::printf("[Kuksa] GetValue %s failed: %s\n", path.c_str(),
                    status.error_message().c_str());
        return std::nullopt;
    }

    if (!response.has_data_point() || !response.data_point().has_value()) {
        return std::nullopt;
    }

    return fromProtoValue(response.data_point().value());
}

}  // namespace body_ecu::adapters

#else  // !HAS_KUKSA_GRPC -- stub implementation for development builds

namespace body_ecu::adapters {

struct KuksaSignalBusAdapter::Impl {};

KuksaSignalBusAdapter::KuksaSignalBusAdapter(const KuksaConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {}

KuksaSignalBusAdapter::~KuksaSignalBusAdapter() = default;

void KuksaSignalBusAdapter::connect() {
    std::printf("[Kuksa] Connecting to %s:%u (stub)\n", config_.host.c_str(),
                config_.port);
    connected_ = true;
}

void KuksaSignalBusAdapter::disconnect() {
    connected_ = false;
}

bool KuksaSignalBusAdapter::publish(const std::string& path,
                                    const ports::SignalValue& value) {
    if (!connected_) return false;
    std::printf("[Kuksa] SET %s (stub)\n", path.c_str());
    (void)value;
    return true;
}

void KuksaSignalBusAdapter::subscribe(const std::string& path,
                                      ports::SignalCallback callback) {
    if (!connected_) return;
    std::printf("[Kuksa] SUBSCRIBE %s (stub)\n", path.c_str());

    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_[path] = callback;
}

std::optional<ports::SignalValue> KuksaSignalBusAdapter::get(
    const std::string& path) const {
    if (!connected_) return std::nullopt;
    std::printf("[Kuksa] GET %s (stub)\n", path.c_str());
    return std::nullopt;
}

}  // namespace body_ecu::adapters

#endif  // HAS_KUKSA_GRPC
