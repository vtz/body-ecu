#include "linux_adapters/StdinButtonAdapter.h"

#include <cstdio>
#include <iostream>

namespace body_ecu::adapters {

StdinButtonAdapter::StdinButtonAdapter() {}

StdinButtonAdapter::~StdinButtonAdapter() {
    stop();
}

void StdinButtonAdapter::start() {
    if (running_) return;
    running_ = true;
    input_thread_ = std::thread([this]() { inputLoop(); });
}

void StdinButtonAdapter::stop() {
    running_ = false;
    if (input_thread_.joinable()) {
        input_thread_.detach();
    }
}

void StdinButtonAdapter::onPress(ports::ButtonCallback callback) {
    callback_ = std::move(callback);
}

void StdinButtonAdapter::inputLoop() {
    std::printf("[Button] Press Enter to simulate button press...\n");
    while (running_) {
        int c = std::getchar();
        if (c == '\n' && callback_) {
            std::printf("[Button] Press detected\n");
            callback_();
        }
        if (c == EOF) break;
    }
}

}  // namespace body_ecu::adapters
