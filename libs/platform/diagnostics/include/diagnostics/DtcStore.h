#pragma once

#include <cstdint>
#include <vector>

namespace body_ecu::platform {

struct Dtc {
    uint32_t code{0};
    uint8_t status_mask{0};
};

class DtcStore {
public:
    void store(uint32_t code, uint8_t status_mask);
    void clear();
    const std::vector<Dtc>& getAll() const { return dtcs_; }

private:
    std::vector<Dtc> dtcs_;
};

}  // namespace body_ecu::platform
