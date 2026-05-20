#include "cloud_gateway/CloudGatewayClient.h"

#include <algorithm>
#include <cstring>

namespace body_ecu::platform {

CloudGatewayClient::CloudGatewayClient(ports::ICloudTransport& transport,
                                       ports::ISignalBus& signal_bus,
                                       const CloudGatewayConfig& config)
    : transport_(transport), signal_bus_(signal_bus), config_(config) {}

std::string CloudGatewayClient::resolveSubject(
    const std::string& pattern) const {
    std::string result = pattern;
    const std::string placeholder = "{vin}";
    auto pos = result.find(placeholder);
    if (pos != std::string::npos) {
        result.replace(pos, placeholder.size(), config_.vin);
    }
    return result;
}

std::string CloudGatewayClient::resolveSubjectWildcard(
    const std::string& pattern) const {
    std::string result = pattern;
    const std::string placeholder = "{vin}";
    auto pos = result.find(placeholder);
    if (pos != std::string::npos) {
        result.replace(pos, placeholder.size(), "*");
    }
    return result;
}

void CloudGatewayClient::init() {
    connected_ = transport_.connect();
    if (!connected_) return;

    transport_.subscribe(
        resolveSubjectWildcard(config_.subject_command_lock),
        [this](const std::string& subject,
               const std::vector<uint8_t>& data) {
            onCloudCommand(subject, data);
        });

    transport_.subscribe(
        resolveSubjectWildcard(config_.subject_command_lights),
        [this](const std::string& /*subject*/,
               const std::vector<uint8_t>& data) {
            if (data.size() >= 2) {
                int32_t packed = (static_cast<int32_t>(data[0]) << 8) | data[1];
                signal_bus_.publish(config_.signal_command_lights,
                                   ports::SignalValue{packed});
            }
        });

    signal_bus_.subscribe(
        config_.signal_is_locked,
        [this](const std::string& path, const ports::SignalValue& value) {
            onLockStateChanged(path, value);
        });

    signal_bus_.subscribe(
        config_.signal_command_response,
        [this](const std::string& path, const ports::SignalValue& value) {
            onCommandResponse(path, value);
        });

    signal_bus_.subscribe(
        config_.signal_mode,
        [this](const std::string& /*path*/, const ports::SignalValue& value) {
            auto* v = std::get_if<int32_t>(&value);
            if (!v) return;
            std::vector<uint8_t> payload = {static_cast<uint8_t>(*v)};
            transport_.publish(resolveSubject(config_.subject_state_mode), payload);
        });

    signal_bus_.subscribe(
        config_.signal_speed,
        [this](const std::string& /*path*/, const ports::SignalValue& value) {
            auto* v = std::get_if<float>(&value);
            if (!v) return;
            uint32_t bits;
            std::memcpy(&bits, v, sizeof(bits));
            std::vector<uint8_t> payload = {
                static_cast<uint8_t>((bits >> 24) & 0xFF),
                static_cast<uint8_t>((bits >> 16) & 0xFF),
                static_cast<uint8_t>((bits >> 8) & 0xFF),
                static_cast<uint8_t>(bits & 0xFF)};
            transport_.publish(resolveSubject(config_.subject_state_speed), payload);
        });

    signal_bus_.subscribe(
        config_.signal_lights,
        [this](const std::string& /*path*/, const ports::SignalValue& value) {
            auto* v = std::get_if<int32_t>(&value);
            if (!v) return;
            std::vector<uint8_t> payload = {static_cast<uint8_t>(*v)};
            transport_.publish(resolveSubject(config_.subject_state_lights), payload);
        });
}

void CloudGatewayClient::publishCurrentState() {
    if (!connected_) return;

    if (auto v = signal_bus_.get(config_.signal_is_locked)) {
        onLockStateChanged(config_.signal_is_locked, *v);
    }
    if (auto v = signal_bus_.get(config_.signal_mode)) {
        if (auto* m = std::get_if<int32_t>(&*v)) {
            std::vector<uint8_t> p = {static_cast<uint8_t>(*m)};
            transport_.publish(resolveSubject(config_.subject_state_mode), p);
        }
    }
    if (auto v = signal_bus_.get(config_.signal_speed)) {
        if (auto* s = std::get_if<float>(&*v)) {
            uint32_t bits;
            std::memcpy(&bits, s, sizeof(bits));
            std::vector<uint8_t> p = {
                static_cast<uint8_t>((bits >> 24) & 0xFF),
                static_cast<uint8_t>((bits >> 16) & 0xFF),
                static_cast<uint8_t>((bits >> 8) & 0xFF),
                static_cast<uint8_t>(bits & 0xFF)};
            transport_.publish(resolveSubject(config_.subject_state_speed), p);
        }
    }
    if (auto v = signal_bus_.get(config_.signal_lights)) {
        if (auto* l = std::get_if<int32_t>(&*v)) {
            std::vector<uint8_t> p = {static_cast<uint8_t>(*l)};
            transport_.publish(resolveSubject(config_.subject_state_lights), p);
        }
    }
}

void CloudGatewayClient::updateVin(const std::string& vin) {
    if (vin.empty() || vin == config_.vin) return;

    config_.vin = vin;

    if (!connected_) return;

    auto subject = "vehicles." + vin + ".info.vin";
    std::vector<uint8_t> vin_bytes(vin.begin(), vin.end());
    transport_.publish(subject, vin_bytes);
    publishCurrentState();
}

void CloudGatewayClient::shutdown() {
    if (connected_) {
        transport_.disconnect();
        connected_ = false;
    }
}

void CloudGatewayClient::onCloudCommand(
    const std::string& /*subject*/, const std::vector<uint8_t>& data) {
    if (data.empty()) return;

    bool lock_requested = data[0] != 0;
    signal_bus_.publish(config_.signal_command_lock,
                        ports::SignalValue{lock_requested});
}

void CloudGatewayClient::onLockStateChanged(
    const std::string& /*path*/, const ports::SignalValue& value) {
    auto* locked = std::get_if<bool>(&value);
    if (!locked) return;

    std::vector<uint8_t> payload = {static_cast<uint8_t>(*locked ? 1 : 0)};
    transport_.publish(resolveSubject(config_.subject_state_locked), payload);
}

void CloudGatewayClient::onCommandResponse(
    const std::string& /*path*/, const ports::SignalValue& value) {
    auto* code = std::get_if<int32_t>(&value);
    if (!code) return;

    std::vector<uint8_t> payload = {static_cast<uint8_t>(*code)};
    transport_.publish(resolveSubject(config_.subject_command_response),
                       payload);
}

}  // namespace body_ecu::platform
