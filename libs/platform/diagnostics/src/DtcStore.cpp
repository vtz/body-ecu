#include "diagnostics/DtcStore.h"

namespace body_ecu::platform {

void DtcStore::store(uint32_t code, uint8_t status_mask) {
    for (auto& dtc : dtcs_) {
        if (dtc.code == code) {
            dtc.status_mask = status_mask;
            return;
        }
    }
    dtcs_.push_back({code, status_mask});
}

void DtcStore::clear() {
    dtcs_.clear();
}

}  // namespace body_ecu::platform
