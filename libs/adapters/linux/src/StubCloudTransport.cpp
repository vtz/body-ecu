#include "linux_adapters/StubCloudTransport.h"

namespace body_ecu::adapters {

bool StubCloudTransport::connect() {
    connected_ = true;
    std::printf("[StubCloud] Connected\n");
    return true;
}

void StubCloudTransport::disconnect() {
    connected_ = false;
    std::printf("[StubCloud] Disconnected\n");
}

bool StubCloudTransport::publish(const std::string& subject,
                                 const std::vector<uint8_t>& data) {
    std::printf("[StubCloud] PUBLISH %s [", subject.c_str());
    for (size_t i = 0; i < data.size(); ++i) {
        std::printf("%s0x%02X", i ? " " : "", data[i]);
    }
    std::printf("]\n");
    return true;
}

void StubCloudTransport::subscribe(const std::string& subject,
                                   ports::CloudMessageCallback callback) {
    std::printf("[StubCloud] SUBSCRIBE %s\n", subject.c_str());
    subscribers_[subject].push_back(std::move(callback));
}

void StubCloudTransport::injectMessage(const std::string& subject,
                                       const std::vector<uint8_t>& data) {
    auto it = subscribers_.find(subject);
    if (it != subscribers_.end()) {
        for (auto& cb : it->second) {
            cb(subject, data);
        }
    }
}

}  // namespace body_ecu::adapters
