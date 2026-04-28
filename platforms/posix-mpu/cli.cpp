#include "cli.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

extern "C" {
#include "third_party/linenoise.h"
}

namespace body_ecu {

Cli::Cli(ports::ISomeIpService& someip, const CliConfig& config)
    : someip_(someip), config_(config) {}

Cli::~Cli() {
    stop();
}

void Cli::init() {
    registerResponseHandler(config_.vehicle_mode_service_id, config_.vehicle_mode_mode_getter);
    registerResponseHandler(config_.vehicle_mode_service_id, config_.vehicle_mode_mode_setter);
    registerResponseHandler(config_.lighting_service_id, config_.lighting_set_light_state_method);
    registerResponseHandler(config_.lighting_service_id, config_.lighting_get_light_status_method);
    registerResponseHandler(config_.door_lock_service_id, config_.door_lock_lock_method);
    registerResponseHandler(config_.door_lock_service_id, config_.door_lock_unlock_method);
    registerResponseHandler(config_.door_lock_service_id, config_.door_lock_get_status_method);
    registerResponseHandler(config_.speed_sensor_service_id, config_.speed_sensor_get_speed_method);
    registerResponseHandler(config_.speed_sensor_service_id, config_.speed_sensor_set_speed_method);
}

void Cli::registerResponseHandler(uint16_t service_id, uint16_t method_id) {
    someip_.registerMethod(service_id, method_id,
        [this](const ports::SomeIpMessage& msg) {
            {
                std::lock_guard<std::mutex> lock(resp_mutex_);
                last_response_ = msg;
                response_ready_ = true;
            }
            resp_cv_.notify_one();
            return msg;
        });
}

ports::SomeIpMessage Cli::sendRequest(uint16_t service_id,
                                       uint16_t method_id,
                                       const std::vector<uint8_t>& payload) {
    {
        std::lock_guard<std::mutex> lock(resp_mutex_);
        response_ready_ = false;
    }

    ports::SomeIpMessage request;
    request.service_id = service_id;
    request.method_id = method_id;
    request.message_type = 0x00;
    request.payload = payload;
    std::printf("[CLI] Sending request svc=0x%04X method=0x%04X\n",
                service_id, method_id);
    someip_.sendResponse(request);

    std::unique_lock<std::mutex> lock(resp_mutex_);
    if (resp_cv_.wait_for(lock, std::chrono::seconds(2),
                          [this] { return response_ready_; })) {
        std::printf("[CLI] Got response svc=0x%04X method=0x%04X rc=0x%02X\n",
                    last_response_.service_id, last_response_.method_id,
                    last_response_.return_code);
        return last_response_;
    }

    std::printf("[CLI] Timeout for svc=0x%04X method=0x%04X\n",
                service_id, method_id);
    ports::SomeIpMessage timeout;
    timeout.return_code = 0xFF;
    return timeout;
}

void Cli::start() {
    running_ = true;
    thread_ = std::thread([this]() { run(); });
}

void Cli::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Cli::run() {
    linenoiseSetMultiLine(0);
    linenoiseHistorySetMaxLen(100);

    std::printf("\n");
    cmdHelp();
    std::printf("\n");

    while (running_) {
        char* line = linenoise("ecu> ");
        if (!line) {
            running_ = false;
            break;
        }

        if (line[0] != '\0') {
            linenoiseHistoryAdd(line);
            processCommand(line);
        }

        linenoiseFree(line);
    }
}

static const char* skipWhitespace(const char* s) {
    while (*s == ' ' || *s == '\t') ++s;
    return s;
}

void Cli::processCommand(const char* line) {
    line = skipWhitespace(line);

    if (std::strncmp(line, "help", 4) == 0 || std::strcmp(line, "?") == 0) {
        cmdHelp();
    } else if (std::strncmp(line, "mode", 4) == 0) {
        cmdMode(skipWhitespace(line + 4));
    } else if (std::strncmp(line, "light", 5) == 0) {
        cmdLight(skipWhitespace(line + 5));
    } else if (std::strncmp(line, "door", 4) == 0) {
        cmdDoor(skipWhitespace(line + 4));
    } else if (std::strncmp(line, "speed", 5) == 0) {
        cmdSpeed(skipWhitespace(line + 5));
    } else if (std::strncmp(line, "status", 6) == 0) {
        cmdStatus();
    } else if (std::strcmp(line, "quit") == 0 ||
               std::strcmp(line, "exit") == 0) {
        running_ = false;
    } else {
        std::printf("Unknown command: '%s'. Type 'help' for usage.\n", line);
    }
}

void Cli::cmdHelp() {
    std::printf(
        "Body ECU CLI commands:\n"
        "  mode [off|accessory|crank|run]  Get or set vehicle mode\n"
        "  light <id> [on|off]             Control lights (headlight, turn, brake)\n"
        "  light all <on|off>              Control all lights at once\n"
        "  door [lock|unlock|status]       Control door lock\n"
        "  speed [<km/h>|clear]            Get/set speed (or 'clear' for ADC)\n"
        "  status                          Show all current states\n"
        "  help                            Show this help\n"
        "  quit                            Exit\n"
    );
}

void Cli::cmdMode(const char* args) {
    if (*args == '\0') {
        auto resp = sendRequest(config_.vehicle_mode_service_id,
                                config_.vehicle_mode_mode_getter);
        if (resp.return_code == 0 && !resp.payload.empty()) {
            std::printf("Vehicle mode: %s\n", modeToString(resp.payload[0]));
        } else if (resp.return_code == 0xFF) {
            std::printf("Timeout waiting for response\n");
        } else {
            std::printf("Failed to get mode (rc=%d)\n", resp.return_code);
        }
        return;
    }

    uint8_t target = 0xFF;
    if (std::strcmp(args, "off") == 0)
        target = 0;
    else if (std::strcmp(args, "accessory") == 0 || std::strcmp(args, "acc") == 0)
        target = 1;
    else if (std::strcmp(args, "run") == 0 || std::strcmp(args, "on") == 0)
        target = 2;
    else if (std::strcmp(args, "crank") == 0)
        target = 3;
    else {
        std::printf("Usage: mode [off|accessory|crank|run]\n");
        return;
    }

    auto resp = sendRequest(config_.vehicle_mode_service_id,
                            config_.vehicle_mode_mode_setter, {target});
    if (resp.return_code == 0) {
        std::printf("Mode set to: %s\n", modeToString(target));
    } else if (resp.return_code == 0xFF) {
        std::printf("Timeout waiting for response\n");
    } else {
        std::printf("Mode transition rejected (rc=%d). Invalid transition?\n",
                    resp.return_code);
    }
}

void Cli::cmdLight(const char* args) {
    if (*args == '\0') {
        auto resp = sendRequest(config_.lighting_service_id,
                                config_.lighting_get_light_status_method);
        if (resp.return_code == 0 && resp.payload.size() >= 3) {
            std::printf("Lights: headlight=%s, turn=%s, brake=%s\n",
                        resp.payload[0] ? "ON" : "OFF",
                        resp.payload[1] ? "ON" : "OFF",
                        resp.payload[2] ? "ON" : "OFF");
        } else if (resp.return_code == 0xFF) {
            std::printf("Timeout waiting for response\n");
        } else {
            std::printf("Failed to get light status (rc=%d)\n",
                        resp.return_code);
        }
        return;
    }

    char name[32] = {};
    char state_str[8] = {};
    if (std::sscanf(args, "%31s %7s", name, state_str) < 2) {
        std::printf("Usage: light <headlight|turn|brake|all> <on|off>\n");
        return;
    }

    uint8_t light_id = 0xFF;
    if (std::strcmp(name, "headlight") == 0 || std::strcmp(name, "head") == 0)
        light_id = 0;
    else if (std::strcmp(name, "turn") == 0)
        light_id = 1;
    else if (std::strcmp(name, "brake") == 0)
        light_id = 2;
    else if (std::strcmp(name, "all") == 0)
        light_id = 0xFE;
    else {
        std::printf("Unknown light: '%s'. Use headlight, turn, brake, or all.\n",
                    name);
        return;
    }

    bool on = (std::strcmp(state_str, "on") == 0 ||
               std::strcmp(state_str, "1") == 0);

    if (light_id == 0xFE) {
        for (uint8_t i = 0; i < 3; ++i) {
            sendRequest(config_.lighting_service_id,
                        config_.lighting_set_light_state_method,
                        {i, on ? uint8_t(1) : uint8_t(0)});
        }
        std::printf("All lights %s\n", on ? "ON" : "OFF");
    } else {
        auto resp = sendRequest(config_.lighting_service_id,
                                config_.lighting_set_light_state_method,
                                {light_id, on ? uint8_t(1) : uint8_t(0)});
        if (resp.return_code == 0) {
            std::printf("%s %s\n", lightIdToString(light_id),
                        on ? "ON" : "OFF");
        } else if (resp.return_code == 0xFF) {
            std::printf("Timeout waiting for response\n");
        } else {
            std::printf("Failed to set light (rc=%d)\n", resp.return_code);
        }
    }
}

void Cli::cmdDoor(const char* args) {
    if (*args == '\0' || std::strcmp(args, "status") == 0) {
        auto resp = sendRequest(config_.door_lock_service_id,
                                config_.door_lock_get_status_method);
        if (resp.return_code == 0 && !resp.payload.empty()) {
            const char* states[] = {"Unlocked", "Locked", "Error"};
            uint8_t s = resp.payload[0];
            std::printf("Door: %s\n", s < 3 ? states[s] : "Unknown");
        } else if (resp.return_code == 0xFF) {
            std::printf("Timeout waiting for response\n");
        } else {
            std::printf("Failed to get door status (rc=%d)\n",
                        resp.return_code);
        }
        return;
    }

    if (std::strcmp(args, "lock") == 0) {
        auto resp = sendRequest(config_.door_lock_service_id,
                                config_.door_lock_lock_method);
        if (resp.return_code == 0xFF)
            std::printf("Timeout\n");
        else
            std::printf("Door %s\n",
                        resp.return_code == 0 ? "locked" : "lock FAILED");
    } else if (std::strcmp(args, "unlock") == 0) {
        auto resp = sendRequest(config_.door_lock_service_id,
                                config_.door_lock_unlock_method);
        if (resp.return_code == 0xFF)
            std::printf("Timeout\n");
        else
            std::printf("Door %s\n",
                        resp.return_code == 0 ? "unlocked" : "unlock FAILED");
    } else {
        std::printf("Usage: door [lock|unlock|status]\n");
    }
}

void Cli::cmdSpeed(const char* args) {
    if (*args == '\0') {
        auto resp = sendRequest(config_.speed_sensor_service_id,
                                config_.speed_sensor_get_speed_method);
        if (resp.return_code == 0 && resp.payload.size() >= 4) {
            std::printf("Speed: %.0f km/h\n",
                        static_cast<double>(deserializeFloat(resp.payload)));
        } else if (resp.return_code == 0xFF) {
            std::printf("Timeout waiting for response\n");
        } else {
            std::printf("Failed to get speed (rc=%d)\n", resp.return_code);
        }
        return;
    }

    if (std::strcmp(args, "clear") == 0 || std::strcmp(args, "adc") == 0) {
        auto payload = serializeFloat(-1.0f);
        auto resp = sendRequest(config_.speed_sensor_service_id,
                                config_.speed_sensor_set_speed_method, payload);
        if (resp.return_code == 0xFF)
            std::printf("Timeout\n");
        else
            std::printf("Speed override %s, resuming ADC\n",
                        resp.return_code == 0 ? "cleared" : "clear FAILED");
        return;
    }

    float value = static_cast<float>(std::atof(args));
    if (value < 0.0f) {
        std::printf("Speed must be >= 0 (or 'clear' to resume ADC)\n");
        return;
    }

    auto payload = serializeFloat(value);
    auto resp = sendRequest(config_.speed_sensor_service_id,
                            config_.speed_sensor_set_speed_method, payload);
    if (resp.return_code == 0) {
        std::printf("Speed set to %.0f km/h\n", static_cast<double>(value));
    } else if (resp.return_code == 0xFF) {
        std::printf("Timeout waiting for response\n");
    } else {
        std::printf("Failed to set speed (rc=%d)\n", resp.return_code);
    }
}

void Cli::cmdStatus() {
    std::printf("--- Vehicle Status ---\n");

    auto mode_resp = sendRequest(config_.vehicle_mode_service_id,
                                 config_.vehicle_mode_mode_getter);
    if (mode_resp.return_code == 0 && !mode_resp.payload.empty()) {
        std::printf("  Mode:      %s\n", modeToString(mode_resp.payload[0]));
    } else {
        std::printf("  Mode:      (unavailable)\n");
    }

    auto speed_resp = sendRequest(config_.speed_sensor_service_id,
                                  config_.speed_sensor_get_speed_method);
    if (speed_resp.return_code == 0 && speed_resp.payload.size() >= 4) {
        std::printf("  Speed:     %.0f km/h\n",
                    static_cast<double>(deserializeFloat(speed_resp.payload)));
    } else {
        std::printf("  Speed:     (unavailable)\n");
    }

    auto light_resp = sendRequest(config_.lighting_service_id,
                                  config_.lighting_get_light_status_method);
    if (light_resp.return_code == 0 && light_resp.payload.size() >= 3) {
        std::printf("  Headlight: %s\n",
                    light_resp.payload[0] ? "ON" : "OFF");
        std::printf("  Turn:      %s\n",
                    light_resp.payload[1] ? "ON" : "OFF");
        std::printf("  Brake:     %s\n",
                    light_resp.payload[2] ? "ON" : "OFF");
    } else {
        std::printf("  Lights:    (unavailable)\n");
    }

    auto door_resp = sendRequest(config_.door_lock_service_id,
                                 config_.door_lock_get_status_method);
    if (door_resp.return_code == 0 && !door_resp.payload.empty()) {
        const char* states[] = {"Unlocked", "Locked", "Error"};
        uint8_t s = door_resp.payload[0];
        std::printf("  Door:      %s\n", s < 3 ? states[s] : "Unknown");
    } else {
        std::printf("  Door:      (unavailable)\n");
    }

    std::printf("----------------------\n");
}

std::vector<uint8_t> Cli::serializeFloat(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return {static_cast<uint8_t>((bits >> 24) & 0xFF),
            static_cast<uint8_t>((bits >> 16) & 0xFF),
            static_cast<uint8_t>((bits >> 8) & 0xFF),
            static_cast<uint8_t>(bits & 0xFF)};
}

float Cli::deserializeFloat(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return 0.0f;
    uint32_t bits = (static_cast<uint32_t>(data[0]) << 24) |
                    (static_cast<uint32_t>(data[1]) << 16) |
                    (static_cast<uint32_t>(data[2]) << 8) |
                     static_cast<uint32_t>(data[3]);
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

const char* Cli::modeToString(uint8_t mode) {
    switch (mode) {
        case 0: return "Off";
        case 1: return "Accessory";
        case 2: return "Run";
        case 3: return "Crank";
        default: return "Unknown";
    }
}

const char* Cli::lightIdToString(uint8_t id) {
    switch (id) {
        case 0: return "Headlight";
        case 1: return "Turn signal";
        case 2: return "Brake light";
        default: return "Unknown light";
    }
}

}  // namespace body_ecu
