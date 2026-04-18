#pragma once

#ifndef BUILD_TESTS

#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>

#include <cstdint>

#include "ports/IAdcInput.h"

namespace body_ecu::adapters {

class AdcAdapter : public ports::IAdcInput {
public:
    explicit AdcAdapter(const struct device* adc_dev);

    bool configure(uint8_t channel, uint8_t resolution = 12);

    int32_t read(uint8_t channel) override;

private:
    const struct device* adc_dev_;
    struct adc_channel_cfg channel_cfg_{};
    uint8_t configured_channel_{0};
    uint8_t resolution_{12};
    bool configured_{false};
};

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
