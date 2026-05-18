#include "speed_simulator/SpeedSimulator.h"

#include <algorithm>
#include <cmath>
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
    someip_.registerMethod(config_.service_id, config_.set_speed_method,
                           [this](const ports::SomeIpMessage& req) {
                               return handleSetSpeed(req);
                           });
    someip_.registerEvent(config_.service_id, config_.speed_changed_event,
                          config_.eventgroup_id);

    timer_id_ = timer_.startPeriodic(config_.update_interval_ms,
                                     [this]() { onTimerTick(); });

    if (timer_id_ == ports::kInvalidTimerId) {
        printk("[speed_sim] FAULT: periodic timer allocation failed\n");
    }

    printk("[speed_sim] Initialized (interval=%ums, max=%d km/h)\n",
           config_.update_interval_ms,
           static_cast<int>(config_.max_speed_kmh));
}

void SpeedSimulator::onModeChanged(ports::VehicleMode /*old_mode*/,
                                   ports::VehicleMode new_mode) {
    current_mode_ = new_mode;
    if (new_mode != ports::VehicleMode::Run) {
        current_speed_kmh_ = 0.0f;
        if (current_speed_kmh_ != last_sent_speed_kmh_) {
            auto payload = serializeFloat(0.0f);
            someip_.sendEvent(config_.service_id, config_.speed_changed_event,
                              payload);
            last_sent_speed_kmh_ = 0.0f;
        }
        if (signal_bus_) {
            signal_bus_->publish(config_.signal_speed,
                                 ports::SignalValue{0.0f});
        }
    }
}

void SpeedSimulator::onTimerTick() {
    if (current_mode_ != ports::VehicleMode::Run) {
        if (current_speed_kmh_ != 0.0f) {
            current_speed_kmh_ = 0.0f;
            auto payload = serializeFloat(0.0f);
            someip_.sendEvent(config_.service_id, config_.speed_changed_event,
                              payload);
            last_sent_speed_kmh_ = 0.0f;
        }
        if (signal_bus_) {
            signal_bus_->publish(config_.signal_speed,
                                 ports::SignalValue{0.0f});
        }
        return;
    }

    if (speed_override_ >= 0.0f) {
        constexpr float kStep = 5.0f;
        current_speed_kmh_ = std::round(speed_override_ / kStep) * kStep;
        current_speed_kmh_ =
            std::clamp(current_speed_kmh_, 0.0f, config_.max_speed_kmh);

        if (current_speed_kmh_ != last_sent_speed_kmh_) {
            auto payload = serializeFloat(current_speed_kmh_);
            someip_.sendEvent(config_.service_id, config_.speed_changed_event,
                              payload);
            last_sent_speed_kmh_ = current_speed_kmh_;
        }
        if (signal_bus_) {
            signal_bus_->publish(config_.signal_speed,
                                 ports::SignalValue{current_speed_kmh_});
        }
        return;
    }

    int32_t raw = adc_.read(config_.adc_channel);
    if (raw < 0) {
        // ADC read failure — hold last known speed rather than driving to 0
        return;
    }
    int32_t clamped = std::clamp(raw, 0, 4095);

    if (clamped < config_.adc_dead_zone) {
        clamped = 0;
    } else if (clamped > (4095 - config_.adc_dead_zone)) {
        clamped = 4095;
    }

    constexpr float kAlpha = 0.03f;
    smoothed_adc_ += kAlpha * (static_cast<float>(clamped) - smoothed_adc_);

    constexpr float kStep = 5.0f;
    float continuous = (smoothed_adc_ / 4095.0f) * config_.max_speed_kmh;
    current_speed_kmh_ =
        std::round(continuous / kStep) * kStep;
    current_speed_kmh_ =
        std::clamp(current_speed_kmh_, 0.0f, config_.max_speed_kmh);

    if (++tick_count_ % 50 == 0) {
        printk("[speed_sim] adc_ch=%u raw=%d smooth=%d speed=%d\n",
               config_.adc_channel, static_cast<int>(raw),
               static_cast<int>(smoothed_adc_),
               static_cast<int>(current_speed_kmh_));
    }

    if (current_speed_kmh_ != last_sent_speed_kmh_) {
        auto payload = serializeFloat(current_speed_kmh_);
        someip_.sendEvent(config_.service_id, config_.speed_changed_event,
                          payload);
        last_sent_speed_kmh_ = current_speed_kmh_;
    }

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

ports::SomeIpMessage SpeedSimulator::handleSetSpeed(
    const ports::SomeIpMessage& request) {
    ports::SomeIpMessage response = request;
    response.message_type = 0x80;

    if (request.payload.size() < 4) {
        response.return_code = 0x01;
        return response;
    }

    float value = deserializeFloat(request.payload);
    if (value < 0.0f) {
        speed_override_ = -1.0f;
        printk("[speed_sim] Override cleared, resuming ADC\n");
    } else {
        speed_override_ = std::clamp(value, 0.0f, config_.max_speed_kmh);
        printk("[speed_sim] Speed override: %d km/h\n",
               static_cast<int>(speed_override_));
    }

    response.return_code = 0x00;
    return response;
}

float SpeedSimulator::deserializeFloat(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return 0.0f;
    uint32_t bits = (static_cast<uint32_t>(data[0]) << 24) |
                    (static_cast<uint32_t>(data[1]) << 16) |
                    (static_cast<uint32_t>(data[2]) << 8) |
                     static_cast<uint32_t>(data[3]);
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
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
