#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "ports/IAdcInput.h"
#include "ports/ISignalBus.h"
#include "ports/ISomeIpService.h"
#include "ports/ITimerService.h"

namespace body_ecu::body {

struct SpeedSimulatorConfig {
    uint16_t service_id{0x1003};
    uint16_t get_speed_method{0x0001};
    uint16_t speed_changed_event{0x8001};
    uint16_t eventgroup_id{0x0001};
    uint8_t adc_channel{0};
    float max_speed_kmh{200.0f};
    float max_acceleration{15.0f};
    float drag_deceleration{3.0f};
    uint32_t update_interval_ms{100};
    std::string signal_speed{"Vehicle.Speed"};
};

class SpeedSimulator {
public:
    SpeedSimulator(ports::IAdcInput& adc, ports::ISomeIpService& someip,
                   ports::ITimerService& timer,
                   const SpeedSimulatorConfig& config = {},
                   ports::ISignalBus* signal_bus = nullptr);

    void init();

    float getSpeedKmh() const { return current_speed_kmh_; }

private:
    void onTimerTick();
    ports::SomeIpMessage handleGetSpeed(const ports::SomeIpMessage& req);
    static std::vector<uint8_t> serializeFloat(float value);

    ports::IAdcInput& adc_;
    ports::ISomeIpService& someip_;
    ports::ITimerService& timer_;
    SpeedSimulatorConfig config_;
    ports::ISignalBus* signal_bus_;
    ports::TimerId timer_id_{0};
    float current_speed_kmh_{0.0f};
    float last_sent_speed_kmh_{-1.0f};
    uint32_t tick_count_{0};
    uint32_t last_sent_tick_{0};
};

}  // namespace body_ecu::body
