#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "ports/ISomeIpService.h"
#include "someip_mpu_config.h"

namespace body_ecu {

using CliConfig = someip::MpuClientConfig;
using VinCallback = std::function<void(const std::string&)>;

class Cli {
public:
    Cli(ports::ISomeIpService& someip, const CliConfig& config = {});
    ~Cli();

    void init();
    void start();
    void stop();
    bool isRunning() const { return running_; }

    void setVinCallback(VinCallback cb) { vin_callback_ = std::move(cb); }

    void registerResponseHandler(uint16_t service_id, uint16_t method_id);
    ports::SomeIpMessage sendRequest(uint16_t service_id, uint16_t method_id,
                                     const std::vector<uint8_t>& payload = {});

private:
    void run();
    void processCommand(const char* line);

    void cmdHelp();
    void cmdMode(const char* args);
    void cmdLight(const char* args);
    void cmdDoor(const char* args);
    void cmdSpeed(const char* args);
    void cmdStatus();

    static std::vector<uint8_t> serializeFloat(float value);
    static float deserializeFloat(const std::vector<uint8_t>& data);
    static const char* modeToString(uint8_t mode);
    static const char* lightIdToString(uint8_t id);

    ports::ISomeIpService& someip_;
    CliConfig config_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    VinCallback vin_callback_;

    std::mutex resp_mutex_;
    std::condition_variable resp_cv_;
    ports::SomeIpMessage last_response_;
    bool response_ready_{false};
};

}  // namespace body_ecu
