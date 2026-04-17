#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "diagnostics/DoIpProtocol.h"
#include "diagnostics/ITransportLayer.h"
#include <lifecycle/SimpleLifecycleComponent.h>

#ifndef __ZEPHYR__
#include <mutex>
#include <thread>
#endif

namespace body_ecu::adapters {

class DoIpTransport : public lifecycle::SimpleLifecycleComponent
                    , public platform::ITransportLayer {
public:
    static constexpr uint16_t kDefaultPort     = 13400;
    static constexpr uint16_t kLogicalAddress  = 0x0E80;

    explicit DoIpTransport(uint16_t port = kDefaultPort) : port_(port) {}
    ~DoIpTransport() override;

    DoIpTransport(const DoIpTransport&) = delete;
    DoIpTransport& operator=(const DoIpTransport&) = delete;

    void setRequestHandler(platform::DiagRequestHandler handler) override;
    void sendResponse(const platform::DiagResponse& response) override;

    bool isConnected() const { return client_fd_.load() >= 0; }
    bool isListening() const { return listen_fd_.load() >= 0; }
    uint16_t port() const { return port_; }

    void init() override;
    void run() override {}
    void shutdown() override;

private:
#ifndef __ZEPHYR__
    void acceptLoop();
    void handleConnection(int fd);
    bool readExact(int fd, uint8_t* buf, size_t len);
    void sendRaw(const std::vector<uint8_t>& data);
#endif

    platform::DiagRequestHandler handler_;

    uint16_t port_;
    std::atomic<int> listen_fd_{-1};
    std::atomic<int> client_fd_{-1};
    std::atomic<bool> running_{false};

#ifndef __ZEPHYR__
    std::thread accept_thread_;
    uint16_t tester_addr_{0};
    std::mutex send_mutex_;
#endif
};

}  // namespace body_ecu::adapters
