#include "speed_simulator/SpeedSimulator.h"

#include <algorithm>
#include <cstring>

#ifdef __ZEPHYR__
#include <zephyr/sys/printk.h>
#else
#include <cstdio>
#define printk(...) std::printf(__VA_ARGS__)
#endif

namespace body_ecu::body {

SpeedSimulator::SpeedSimulator(ports::IAdcInput& adc,
                               ports::ISomeIpService& someip,
                               ports::ITimerService& timer,
                               const SpeedSimulatorConfig& config,
                               ports::ISignalBus* signal_bus)
    : adc_(adc),
      someip_(someip),
      timer_(timer),
      config_(config),
      signal_bus_(signal_bus) {}

void SpeedSimulator::init() {
    someip_.registerMethod(config_.service_id, config_.get_speed_method,
                           [this](const ports::SomeIpMessage& req) {
                               return handleGetSpeed(req);
                           });
    someip_.registerEvent(config_.service_id, config_.speed_changed_event,
                          config_.eventgroup_id);

    timer_id_ = timer_.startPeriodic(config_.update_interval_ms,
                                     [this]() { onTimerTick(); });

    printk("[speed_sim] Initialized (interval=%ums, max=%d km/h)\n",
           config_.update_interval_ms,
           static_cast<int>(config_.max_speed_kmh));
}

void SpeedSimulator::onTimerTick() {
    int32_t raw = adc_.read(config_.adc_channel);
    float throttle =
        static_cast<float>(std::clamp(raw, 0, 4095)) / 4095.0f;

    float dt = static_cast<float>(config_.update_interval_ms) / 1000.0f;
    float accel_mps2 = (throttle > 0.01f)
                           ? throttle * config_.max_acceleration
                           : -config_.drag_deceleration;

    current_speed_kmh_ += accel_mps2 * dt * 3.6f;
    current_speed_kmh_ =
        std::clamp(current_speed_kmh_, 0.0f, config_.max_speed_kmh);

    auto payload = serializeFloat(current_speed_kmh_);
    someip_.sendEvent(config_.service_id, config_.speed_changed_event,
                      payload);

    if (signal_bus_) {
        signal_bus_->publish(config_.signal_speed,
                             ports::SignalValue{current_speed_kmh_});
    }
}

ports::SomeIpMessage SpeedSimulator::handleGetSpeed(
    const ports::SomeIpMessage& request) {
    ports::SomeIpMessage response = request;
    response.message_type = 0x80;
    response.return_code = 0x00;
    response.payload = serializeFloat(current_speed_kmh_);
    return response;
}

std::vector<uint8_t> SpeedSimulator::serializeFloat(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return {static_cast<uint8_t>((bits >> 24) & 0xFF),
            static_cast<uint8_t>((bits >> 16) & 0xFF),
            static_cast<uint8_t>((bits >> 8) & 0xFF),
            static_cast<uint8_t>(bits & 0xFF)};
}

}  // namespace body_ecu::body
