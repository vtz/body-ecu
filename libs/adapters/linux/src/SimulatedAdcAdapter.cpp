#include "linux_adapters/SimulatedAdcAdapter.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

namespace body_ecu::adapters {

SimulatedAdcAdapter::SimulatedAdcAdapter(const std::string& file_path)
    : file_path_(file_path) {}

int32_t SimulatedAdcAdapter::read(uint8_t /*channel*/) {
    int32_t ov = override_value_.load();
    if (ov >= 0) {
        return std::clamp(ov, 0, 4095);
    }

    std::ifstream f(file_path_);
    if (f.is_open()) {
        int32_t value = 0;
        if (f >> value) {
            return std::clamp(value, 0, 4095);
        }
    }

    return 0;
}

}  // namespace body_ecu::adapters
