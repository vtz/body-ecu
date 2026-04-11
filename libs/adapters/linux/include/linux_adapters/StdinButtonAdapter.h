#pragma once

#include <atomic>
#include <thread>

#include "ports/IButtonInput.h"

namespace body_ecu::adapters {

/// Linux/POSIX button adapter that listens for Enter key on stdin.
/// Pressing Enter simulates a button press event.
class StdinButtonAdapter : public ports::IButtonInput {
public:
    StdinButtonAdapter();
    ~StdinButtonAdapter();

    void start();
    void stop();

    void onPress(ports::ButtonCallback callback) override;

private:
    void inputLoop();

    ports::ButtonCallback callback_;
    std::atomic<bool> running_{false};
    std::thread input_thread_;
};

}  // namespace body_ecu::adapters
