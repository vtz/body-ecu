#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "ports/IAdcInput.h"

namespace body_ecu::adapters {

/// POSIX ADC adapter that reads throttle from a file or programmatic value.
/// Write a raw ADC value (0-4095) to the file to simulate the potentiometer:
///   echo 2048 > /tmp/body_ecu_throttle
class SimulatedAdcAdapter : public ports::IAdcInput {
public:
    explicit SimulatedAdcAdapter(
        const std::string& file_path = "/tmp/body_ecu_throttle");

    int32_t read(uint8_t channel) override;

    void setValue(int32_t value) { override_value_ = value; }

private:
    std::string file_path_;
    std::atomic<int32_t> override_value_{-1};
};

}  // namespace body_ecu::adapters
