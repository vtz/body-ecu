#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "ports/IAdcInput.h"
#include "ports/IModeObserver.h"
#include "ports/ISignalBus.h"
#include "ports/ISomeIpService.h"
#include "ports/ITimerService.h"
#include "someip_service_ids.h"

namespace body_ecu::body {

struct SpeedSimulatorConfig {
    uint16_t service_id{someip::speed_sensor::kServiceId};
    uint16_t get_speed_method{someip::speed_sensor::method::kGetSpeed};
    uint16_t set_speed_method{someip::speed_sensor::method::kSetSpeed};
    uint16_t speed_changed_event{someip::speed_sensor::event::kSpeedChanged};
    uint16_t eventgroup_id{someip::speed_sensor::eventgroup::kSpeedEvents};
    uint8_t adc_channel{0};
    float max_speed_kmh{200.0f};
    int32_t adc_dead_zone{200};
    uint32_t update_interval_ms{100};
    std::string signal_speed{"Vehicle.Speed"};
};

class SpeedSimulator : public ports::IModeObserver {
public:
    SpeedSimulator(ports::IAdcInput& adc, ports::ISomeIpService& someip,
                   ports::ITimerService& timer,
                   const SpeedSimulatorConfig& config = {},
                   ports::ISignalBus* signal_bus = nullptr);

    void init();

    float getSpeedKmh() const { return current_speed_kmh_; }

    // IModeObserver
    void onModeChanged(ports::VehicleMode old_mode,
                       ports::VehicleMode new_mode) override;

private:
    void onTimerTick();
    ports::SomeIpMessage handleGetSpeed(const ports::SomeIpMessage& req);
    ports::SomeIpMessage handleSetSpeed(const ports::SomeIpMessage& req);
    static std::vector<uint8_t> serializeFloat(float value);
    static float deserializeFloat(const std::vector<uint8_t>& data);

    ports::IAdcInput& adc_;
    ports::ISomeIpService& someip_;
    ports::ITimerService& timer_;
    SpeedSimulatorConfig config_;
    ports::ISignalBus* signal_bus_;
    ports::TimerId timer_id_{0};
    float current_speed_kmh_{0.0f};
    float smoothed_adc_{0.0f};
    float last_sent_speed_kmh_{0.0f};
    uint32_t tick_count_{0};
    float speed_override_{-1.0f};
    ports::VehicleMode current_mode_{ports::VehicleMode::Off};
};

}  // namespace body_ecu::body
