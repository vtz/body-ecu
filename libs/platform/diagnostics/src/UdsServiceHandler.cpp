#include "diagnostics/UdsServiceHandler.h"

namespace body_ecu::platform {

namespace {
constexpr uint8_t kSidDiagSessionControl = 0x10;
constexpr uint8_t kSidReadDataById = 0x22;
constexpr uint8_t kSidIoControl = 0x2F;
constexpr uint8_t kSidReadDtc = 0x19;

constexpr uint8_t kNrcServiceNotSupported = 0x11;
constexpr uint8_t kNrcSubFunctionNotSupported = 0x12;
constexpr uint8_t kNrcRequestOutOfRange = 0x31;
constexpr uint8_t kNrcConditionsNotCorrect = 0x22;
constexpr uint8_t kPositiveResponseOffset = 0x40;
}  // namespace

UdsServiceHandler::UdsServiceHandler(DtcStore& dtc_store)
    : dtc_store_(dtc_store) {}

void UdsServiceHandler::addProvider(ports::IDiagDataProvider* provider) {
    providers_.push_back(provider);
}

DiagResponse UdsServiceHandler::handleRequest(const DiagRequest& request) {
    if (request.empty()) {
        return negativeResponse(0x00, kNrcServiceNotSupported);
    }

    uint8_t sid = request[0];
    switch (sid) {
        case kSidDiagSessionControl:
            return handleDiagSessionControl(request);
        case kSidReadDataById:
            return handleReadDataById(request);
        case kSidIoControl:
            return handleIoControl(request);
        case kSidReadDtc:
            return handleReadDtc(request);
        default:
            return negativeResponse(sid, kNrcServiceNotSupported);
    }
}

DiagResponse UdsServiceHandler::handleDiagSessionControl(
    const DiagRequest& request) {
    if (request.size() < 2) {
        return negativeResponse(kSidDiagSessionControl,
                                kNrcSubFunctionNotSupported);
    }

    uint8_t sub = request[1];
    if (sub == 0x01) {
        session_ = DiagSession::Default;
    } else if (sub == 0x03) {
        session_ = DiagSession::Extended;
    } else {
        return negativeResponse(kSidDiagSessionControl,
                                kNrcSubFunctionNotSupported);
    }

    return {static_cast<uint8_t>(kSidDiagSessionControl +
                                 kPositiveResponseOffset),
            sub};
}

DiagResponse UdsServiceHandler::handleReadDataById(
    const DiagRequest& request) {
    if (request.size() < 3) {
        return negativeResponse(kSidReadDataById, kNrcRequestOutOfRange);
    }

    uint16_t did =
        (static_cast<uint16_t>(request[1]) << 8) | request[2];

    for (auto* provider : providers_) {
        auto data = provider->readData(did);
        if (data.has_value()) {
            DiagResponse resp;
            resp.push_back(kSidReadDataById + kPositiveResponseOffset);
            resp.push_back(request[1]);
            resp.push_back(request[2]);
            resp.insert(resp.end(), data->data.begin(), data->data.end());
            return resp;
        }
    }

    return negativeResponse(kSidReadDataById, kNrcRequestOutOfRange);
}

DiagResponse UdsServiceHandler::handleIoControl(
    const DiagRequest& request) {
    if (session_ != DiagSession::Extended) {
        return negativeResponse(kSidIoControl, kNrcConditionsNotCorrect);
    }

    if (request.size() < 4) {
        return negativeResponse(kSidIoControl, kNrcRequestOutOfRange);
    }

    uint16_t did =
        (static_cast<uint16_t>(request[1]) << 8) | request[2];
    std::vector<uint8_t> control_param(request.begin() + 3, request.end());

    for (auto* provider : providers_) {
        if (provider->ioControl(did, control_param)) {
            return {static_cast<uint8_t>(kSidIoControl +
                                         kPositiveResponseOffset),
                    request[1], request[2]};
        }
    }

    return negativeResponse(kSidIoControl, kNrcRequestOutOfRange);
}

DiagResponse UdsServiceHandler::handleReadDtc(const DiagRequest& request) {
    if (request.size() < 2) {
        return negativeResponse(kSidReadDtc, kNrcSubFunctionNotSupported);
    }

    DiagResponse resp;
    resp.push_back(kSidReadDtc + kPositiveResponseOffset);
    resp.push_back(request[1]);

    const auto& dtcs = dtc_store_.getAll();
    for (const auto& dtc : dtcs) {
        resp.push_back(static_cast<uint8_t>((dtc.code >> 16) & 0xFF));
        resp.push_back(static_cast<uint8_t>((dtc.code >> 8) & 0xFF));
        resp.push_back(static_cast<uint8_t>(dtc.code & 0xFF));
        resp.push_back(dtc.status_mask);
    }

    return resp;
}

DiagResponse UdsServiceHandler::negativeResponse(uint8_t sid, uint8_t nrc) {
    return {0x7F, sid, nrc};
}

}  // namespace body_ecu::platform
