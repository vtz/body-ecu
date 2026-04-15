#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "ports/IDiagDataProvider.h"

namespace body_ecu::adapters {

class VehicleInfoProvider : public ports::IDiagDataProvider {
public:
    static constexpr uint16_t kDidVin       = 0xF190;
    static constexpr uint16_t kDidEcuSerial = 0xF18C;
    static constexpr size_t   kVinLength    = 17;

    void setVin(const char* vin) {
        std::memset(vin_.data(), 0, vin_.size());
        auto len = std::strlen(vin);
        if (len > kVinLength) len = kVinLength;
        std::memcpy(vin_.data(), vin, len);
    }

    void setEcuSerial(const char* serial) {
        ecu_serial_.assign(serial, serial + std::strlen(serial));
    }

    std::optional<ports::DiagData> readData(uint16_t did) const override {
        if (did == kDidVin) {
            ports::DiagData d;
            d.did = did;
            d.data.assign(vin_.begin(), vin_.end());
            return d;
        }
        if (did == kDidEcuSerial && !ecu_serial_.empty()) {
            ports::DiagData d;
            d.did = did;
            d.data = ecu_serial_;
            return d;
        }
        return std::nullopt;
    }

    bool ioControl(uint16_t /*did*/,
                   const std::vector<uint8_t>& /*control_param*/) override {
        return false;
    }

private:
    std::array<uint8_t, kVinLength> vin_{'0','0','0','0','0','0','0','0',
                                          '0','0','0','0','0','0','0','0','0'};
    std::vector<uint8_t> ecu_serial_;
};

}  // namespace body_ecu::adapters
