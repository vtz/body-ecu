#pragma once

#include <cstdint>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

#include "ports/ICanBus.h"

namespace body_ecu::adapters {

/// Linux SocketCAN adapter implementing ICanBus.
/// Uses the Linux CAN socket interface (AF_CAN) for send/receive.
/// Supports both classical CAN and CAN-FD frames.
///
/// Usage: create a vcan interface first:
///   sudo modprobe vcan
///   sudo ip link add dev vcan0 type vcan
///   sudo ip link set up vcan0
class SocketCanAdapter : public ports::ICanBus {
public:
    explicit SocketCanAdapter(const std::string& iface = "vcan0");
    ~SocketCanAdapter();

    bool open();
    void close();
    bool isOpen() const { return fd_ >= 0; }

    bool send(const ports::CanFrame& frame) override;
    void addRxCallback(ports::CanRxCallback callback) override;

private:
    void rxLoop();

    std::string iface_;
    int fd_{-1};
    std::vector<ports::CanRxCallback> rx_callbacks_;
    std::atomic<bool> running_{false};
    std::thread rx_thread_;
};

}  // namespace body_ecu::adapters
