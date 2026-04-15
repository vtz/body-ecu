#include "DoIpTransport.h"

#include <cstdio>

#ifdef __ZEPHYR__

namespace body_ecu::adapters {

DoIpTransport::~DoIpTransport() { shutdown(); }

void DoIpTransport::setRequestHandler(platform::DiagRequestHandler handler) {
    handler_ = std::move(handler);
}

void DoIpTransport::init() {
    // DoIP TCP server not yet implemented on Zephyr.
    // Requires CONFIG_NET_SOCKETS + Zephyr threading.
    running_.store(true);
    std::printf("[DoIP] Stub on Zephyr (TCP server not available)\n");
}

void DoIpTransport::shutdown() {
    running_.store(false);
    handler_ = nullptr;
}

void DoIpTransport::sendResponse(const platform::DiagResponse&) {}

}  // namespace body_ecu::adapters

#else  // POSIX

#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace body_ecu::adapters {

using namespace platform::doip;

DoIpTransport::~DoIpTransport() {
    shutdown();
}

void DoIpTransport::setRequestHandler(platform::DiagRequestHandler handler) {
    handler_ = std::move(handler);
}

void DoIpTransport::init() {
    if (running_) return;

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::printf("[DoIP] Failed to create socket: %s\n", std::strerror(errno));
        return;
    }

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::printf("[DoIP] bind failed on port %u: %s\n", port_,
                    std::strerror(errno));
        ::close(fd);
        return;
    }

    if (::listen(fd, 1) < 0) {
        std::printf("[DoIP] listen failed: %s\n", std::strerror(errno));
        ::close(fd);
        return;
    }

    listen_fd_.store(fd);
    running_.store(true);
    accept_thread_ = std::thread(&DoIpTransport::acceptLoop, this);

    std::printf("[DoIP] Listening on 0.0.0.0:%u (entity 0x%04X)\n",
                port_, kLogicalAddress);
}

void DoIpTransport::shutdown() {
    running_.store(false);

    int cfd = client_fd_.exchange(-1);
    if (cfd >= 0) ::close(cfd);

    int lfd = listen_fd_.exchange(-1);
    if (lfd >= 0) ::close(lfd);

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    handler_ = nullptr;
    std::printf("[DoIP] Shut down\n");
}

void DoIpTransport::acceptLoop() {
    while (running_) {
        struct sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);
        int lfd = listen_fd_.load();
        if (lfd < 0) break;

        int cfd = ::accept(lfd, reinterpret_cast<struct sockaddr*>(&peer),
                           &peer_len);
        if (cfd < 0) {
            if (!running_) break;
            std::printf("[DoIP] accept error: %s\n", std::strerror(errno));
            continue;
        }

        char peer_ip[INET_ADDRSTRLEN]{};
        ::inet_ntop(AF_INET, &peer.sin_addr, peer_ip, sizeof(peer_ip));
        std::printf("[DoIP] Connection from %s:%u\n", peer_ip,
                    ntohs(peer.sin_port));

        int old = client_fd_.exchange(cfd);
        if (old >= 0) ::close(old);

        handleConnection(cfd);
        client_fd_.store(-1);
        ::close(cfd);
        std::printf("[DoIP] Client disconnected\n");
    }
}

void DoIpTransport::handleConnection(int fd) {
    uint8_t hdr_buf[kHeaderLen];

    while (running_) {
        if (!readExact(fd, hdr_buf, kHeaderLen)) break;

        auto hdr = parseHeader(hdr_buf);

        if (hdr.version != kProtocolVersion || hdr.inverse != kInverseVersion) {
            auto nack = serializeHeader(PayloadType::GenericNack, 1);
            nack.push_back(0x00);
            sendRaw(nack);
            break;
        }

        if (hdr.payload_length > 64 * 1024) break;

        std::vector<uint8_t> payload(hdr.payload_length);
        if (hdr.payload_length > 0 &&
            !readExact(fd, payload.data(), hdr.payload_length)) {
            break;
        }

        auto type = static_cast<PayloadType>(hdr.payload_type);

        switch (type) {
        case PayloadType::RoutingActivationRequest: {
            if (payload.size() < 7) break;
            tester_addr_ = static_cast<uint16_t>(
                (payload[0] << 8) | payload[1]);
            auto resp = makeRoutingActivationResponse(
                tester_addr_, kLogicalAddress,
                RoutingActivationCode::Success);
            sendRaw(resp);
            std::printf("[DoIP] Routing activated for tester 0x%04X\n",
                        tester_addr_);
            break;
        }

        case PayloadType::DiagnosticMessage: {
            if (payload.size() < 5) break;
            uint16_t src = static_cast<uint16_t>(
                (payload[0] << 8) | payload[1]);
            uint16_t tgt = static_cast<uint16_t>(
                (payload[2] << 8) | payload[3]);

            auto ack = makeDiagnosticAck(tgt, src, 0x00);
            sendRaw(ack);

            std::vector<uint8_t> uds_data(payload.begin() + 4, payload.end());

            if (handler_) {
                auto uds_resp = handler_(uds_data);
                auto diag_msg = makeDiagnosticMessage(
                    kLogicalAddress, src, uds_resp);
                sendRaw(diag_msg);
            }
            (void)tgt;
            break;
        }

        case PayloadType::AliveCheckRequest: {
            auto resp = serializeHeader(PayloadType::AliveCheckResponse, 2);
            resp.push_back(static_cast<uint8_t>(kLogicalAddress >> 8));
            resp.push_back(static_cast<uint8_t>(kLogicalAddress & 0xFF));
            sendRaw(resp);
            break;
        }

        default:
            break;
        }
    }
}

bool DoIpTransport::readExact(int fd, uint8_t* buf, size_t len) {
    size_t total = 0;
    while (total < len && running_) {
        auto n = ::recv(fd, buf + total, len - total, 0);
        if (n <= 0) return false;
        total += static_cast<size_t>(n);
    }
    return total == len;
}

void DoIpTransport::sendRaw(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    int fd = client_fd_.load();
    if (fd < 0) return;

    size_t total = 0;
    while (total < data.size()) {
        auto n = ::send(fd, data.data() + total, data.size() - total, 0);
        if (n <= 0) break;
        total += static_cast<size_t>(n);
    }
}

void DoIpTransport::sendResponse(const platform::DiagResponse& response) {
    auto msg = makeDiagnosticMessage(kLogicalAddress, tester_addr_, response);
    sendRaw(msg);
}

}  // namespace body_ecu::adapters

#endif  // __ZEPHYR__
