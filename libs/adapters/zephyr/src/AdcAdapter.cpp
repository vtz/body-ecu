#ifndef BUILD_TESTS

#include "zephyr_adapters/AdcAdapter.h"

#include <zephyr/sys/printk.h>

namespace body_ecu::adapters {

AdcAdapter::AdcAdapter(const struct device* adc_dev) : adc_dev_(adc_dev) {}

bool AdcAdapter::configure(uint8_t channel, uint8_t resolution) {
    if (!device_is_ready(adc_dev_)) {
        printk("[adc] Device not ready\n");
        return false;
    }

    configured_channel_ = channel;
    resolution_ = resolution;

    channel_cfg_.gain = ADC_GAIN_1;
    channel_cfg_.reference = ADC_REF_INTERNAL;
    channel_cfg_.acquisition_time =
        ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 811);
    channel_cfg_.channel_id = channel;

    int ret = adc_channel_setup(adc_dev_, &channel_cfg_);
    if (ret < 0) {
        printk("[adc] Channel setup failed: %d\n", ret);
        return false;
    }

    configured_ = true;
    printk("[adc] Configured channel %u (%u-bit)\n", channel, resolution);
    return true;
}

int32_t AdcAdapter::read(uint8_t channel) {
    if (!configured_ || channel != configured_channel_) {
        return 0;
    }

    int16_t sample = 0;
    struct adc_sequence sequence = {
        .options = nullptr,
        .channels = BIT(channel),
        .buffer = &sample,
        .buffer_size = sizeof(sample),
        .resolution = resolution_,
        .oversampling = 0,
        .calibrate = false,
    };

    int ret = adc_read(adc_dev_, &sequence);
    if (ret < 0) {
        printk("[adc] read failed: %d\n", ret);
        return 0;
    }

    return static_cast<int32_t>(sample);
}

}  // namespace body_ecu::adapters

#endif  // BUILD_TESTS
