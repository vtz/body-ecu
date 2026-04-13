#include "cloud_gateway/CloudGatewayClient.h"

#include <algorithm>

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

void CloudGatewayClient::init() {
    connected_ = transport_.connect();
    if (!connected_) return;

    transport_.subscribe(
        resolveSubject(config_.subject_command_lock),
        [this](const std::string& subject,
               const std::vector<uint8_t>& data) {
            onCloudCommand(subject, data);
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
