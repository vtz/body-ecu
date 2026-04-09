#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace body_ecu::platform {

using DiagRequest = std::vector<uint8_t>;
using DiagResponse = std::vector<uint8_t>;
using DiagRequestHandler = std::function<DiagResponse(const DiagRequest&)>;

class ITransportLayer {
public:
    virtual ~ITransportLayer() = default;
    virtual void setRequestHandler(DiagRequestHandler handler) = 0;
    virtual void sendResponse(const DiagResponse& response) = 0;
};

}  // namespace body_ecu::platform
