#pragma once

#include <optional>
#include <string>

#include "config_loader/GatewayConfig.h"
#include "config_loader/ServiceConfig.h"

namespace body_ecu::config {

class ConfigLoader {
public:
    /// Parse services config from a YAML string.
    static std::optional<ServicesConfig> parseServices(
        const std::string& yaml_content);

    /// Parse gateway config from a YAML string.
    static std::optional<GatewayConfig> parseGateway(
        const std::string& yaml_content);

    /// Load services config from file, with env var overrides.
    static std::optional<ServicesConfig> loadServices(
        const std::string& file_path);

    /// Load gateway config from file.
    static std::optional<GatewayConfig> loadGateway(
        const std::string& file_path);

    /// Parse a hex string like "0x1000" to uint16_t.
    static uint16_t parseHex16(const std::string& s);

    /// Parse a hex string like "0x200" to uint32_t.
    static uint32_t parseHex32(const std::string& s);

private:
    static void applyEnvOverrides(ServicesConfig& config);
};

}  // namespace body_ecu::config
