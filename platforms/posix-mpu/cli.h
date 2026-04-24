#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "ports/ISomeIpService.h"

namespace body_ecu {

struct CliConfig {
    uint16_t mode_service_id{0x1002};
    uint16_t mode_get_method{0x0001};
    uint16_t mode_set_method{0x0002};

    uint16_t lighting_service_id{0x1000};
    uint16_t lighting_set_method{0x0001};
    uint16_t lighting_get_method{0x0002};

    uint16_t door_service_id{0x1001};
    uint16_t door_lock_method{0x0001};
    uint16_t door_unlock_method{0x0002};
    uint16_t door_get_method{0x0003};

    uint16_t speed_service_id{0x1003};
    uint16_t speed_get_method{0x0001};
    uint16_t speed_set_method{0x0002};
};

class Cli {
public:
    Cli(ports::ISomeIpService& someip, const CliConfig& config = {});
    ~Cli();

    void init();
    void start();
    void stop();
    bool isRunning() const { return running_; }

private:
    void run();
    void processCommand(const char* line);

    void cmdHelp();
    void cmdMode(const char* args);
    void cmdLight(const char* args);
    void cmdDoor(const char* args);
    void cmdSpeed(const char* args);
    void cmdStatus();

    void registerResponseHandler(uint16_t service_id, uint16_t method_id);
    ports::SomeIpMessage sendRequest(uint16_t service_id, uint16_t method_id,
                                     const std::vector<uint8_t>& payload = {});

    static std::vector<uint8_t> serializeFloat(float value);
    static float deserializeFloat(const std::vector<uint8_t>& data);
    static const char* modeToString(uint8_t mode);
    static const char* lightIdToString(uint8_t id);

    ports::ISomeIpService& someip_;
    CliConfig config_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    std::mutex resp_mutex_;
    std::condition_variable resp_cv_;
    ports::SomeIpMessage last_response_;
    bool response_ready_{false};
};

}  // namespace body_ecu
