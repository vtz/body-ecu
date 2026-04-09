#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace body_ecu::ports {

struct DiagData {
    uint16_t did{0};
    std::vector<uint8_t> data;
};

class IDiagDataProvider {
public:
    virtual ~IDiagDataProvider() = default;
    virtual std::optional<DiagData> readData(uint16_t did) const = 0;
    virtual bool ioControl(uint16_t did,
                           const std::vector<uint8_t>& control_param) = 0;
};

}  // namespace body_ecu::ports
