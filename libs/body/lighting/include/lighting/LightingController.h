#pragma once

#include <array>
#include <cstdint>

#include "ports/IDiagDataProvider.h"
#include "ports/IGpioPort.h"
#include "ports/IModeObserver.h"
#include "ports/ISomeIpService.h"
#include "someip_service_ids.h"

namespace body_ecu::body {

enum class LightId : uint8_t {
    Headlight = 0,
    Turn = 1,
    Brake = 2,
};
static constexpr size_t kLightCount = 3;

struct LightingConfig {
    uint16_t service_id{someip::lighting::kServiceId};
    uint16_t set_light_state_method{someip::lighting::method::kSetLightState};
    uint16_t get_light_status_method{someip::lighting::method::kGetLightStatus};
    uint16_t light_status_changed_event{someip::lighting::event::kLightStatusChanged};
    uint16_t eventgroup_id{someip::lighting::eventgroup::kLightingEvents};
    std::array<uint32_t, kLightCount> gpio_pins{0, 1, 2};
    uint16_t diag_did{0xF100};
};

class LightingController : public ports::IModeObserver,
                           public ports::IDiagDataProvider {
public:
    LightingController(ports::IGpioPort& gpio, ports::ISomeIpService& someip,
                       const LightingConfig& config = {});

    void init();

    bool setLightState(LightId id, bool on);
    std::array<bool, kLightCount> getLightStatus() const;

    // IModeObserver
    void onModeChanged(ports::VehicleMode old_mode,
                       ports::VehicleMode new_mode) override;

    // IDiagDataProvider
    std::optional<ports::DiagData> readData(uint16_t did) const override;
    bool ioControl(uint16_t did,
                   const std::vector<uint8_t>& control_param) override;

private:
    ports::SomeIpMessage handleSetLightState(
        const ports::SomeIpMessage& request);
    ports::SomeIpMessage handleGetLightStatus(
        const ports::SomeIpMessage& request);
    void publishLightStatusChanged();

    ports::IGpioPort& gpio_;
    ports::ISomeIpService& someip_;
    LightingConfig config_;
    std::array<bool, kLightCount> states_{};
};

}  // namespace body_ecu::body
