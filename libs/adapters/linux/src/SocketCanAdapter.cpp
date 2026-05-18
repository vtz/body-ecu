#include "linux_adapters/SocketCanAdapter.h"

#include <cstdio>
#include <cstring>

#ifdef __linux__
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace body_ecu::adapters {

SocketCanAdapter::SocketCanAdapter(const std::string& iface)
    : iface_(iface) {}

SocketCanAdapter::~SocketCanAdapter() {
    close();
}

bool SocketCanAdapter::open() {
#ifdef __linux__
    fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd_ < 0) {
        std::perror("[SocketCAN] socket");
        return false;
    }

    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, iface_.c_str(), IFNAMSIZ - 1);
    if (ioctl(fd_, SIOCGIFINDEX, &ifr) < 0) {
        std::perror("[SocketCAN] ioctl SIOCGIFINDEX");
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    struct sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("[SocketCAN] bind");
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    std::printf("[SocketCAN] Opened %s (fd=%d)\n", iface_.c_str(), fd_);
    return true;
#else
    std::printf("[SocketCAN] Not available on this platform (Linux only)\n");
    return false;
#endif
}

void SocketCanAdapter::close() {
    running_ = false;
    if (rx_thread_.joinable()) {
        rx_thread_.join();
    }
#ifdef __linux__
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

bool SocketCanAdapter::send(const ports::CanFrame& frame) {
#ifdef __linux__
    if (fd_ < 0) return false;

    struct can_frame cf {};
    cf.can_id = frame.id;
    cf.can_dlc = frame.dlc;
    std::memcpy(cf.data, frame.data, std::min<size_t>(frame.dlc, 8));

    ssize_t nbytes = ::write(fd_, &cf, sizeof(cf));
    if (nbytes != sizeof(cf)) {
        std::perror("[SocketCAN] write");
        return false;
    }
    return true;
#else
    std::printf("[SocketCAN] send(id=0x%03X, dlc=%u) [stub]\n",
                frame.id, frame.dlc);
    return true;
#endif
}

void SocketCanAdapter::addRxCallback(ports::CanRxCallback callback) {
    rx_callbacks_.push_back(std::move(callback));

    if (!running_ && fd_ >= 0) {
        running_ = true;
        rx_thread_ = std::thread([this]() { rxLoop(); });
    }
}

void SocketCanAdapter::rxLoop() {
#ifdef __linux__
    while (running_) {
        struct can_frame cf {};
        ssize_t nbytes = ::read(fd_, &cf, sizeof(cf));
        if (nbytes < 0) {
            if (running_) std::perror("[SocketCAN] read");
            break;
        }
        if (nbytes == sizeof(cf) && !rx_callbacks_.empty()) {
            ports::CanFrame pf;
            pf.id = cf.can_id & CAN_EFF_MASK;
            uint8_t safe_dlc = std::min(cf.can_dlc,
                                        static_cast<__u8>(sizeof(pf.data)));
            pf.dlc = safe_dlc;
            std::memcpy(pf.data, cf.data, safe_dlc);
            for (auto& cb : rx_callbacks_) {
                if (cb) cb(pf);
            }
        }
    }
#endif
}

}  // namespace body_ecu::adapters
