#include "config_loader/ConfigLoader.h"

#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace body_ecu::config {

namespace {

uint16_t nodeToU16(const YAML::Node& node) {
    if (node.IsScalar()) {
        auto s = node.as<std::string>();
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            return ConfigLoader::parseHex16(s);
        }
        return static_cast<uint16_t>(node.as<int>());
    }
    return 0;
}

uint32_t nodeToU32(const YAML::Node& node) {
    if (node.IsScalar()) {
        auto s = node.as<std::string>();
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            return ConfigLoader::parseHex32(s);
        }
        return static_cast<uint32_t>(node.as<int>());
    }
    return 0;
}

}  // namespace

uint16_t ConfigLoader::parseHex16(const std::string& s) {
    return static_cast<uint16_t>(std::stoul(s, nullptr, 0));
}

uint32_t ConfigLoader::parseHex32(const std::string& s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 0));
}

std::optional<ServicesConfig> ConfigLoader::parseServices(
    const std::string& yaml_content) {
    try {
        auto root = YAML::Load(yaml_content);
        ServicesConfig cfg;

        if (auto net = root["network"]) {
            if (net["host"]) cfg.network.host = net["host"].as<std::string>();
            if (net["port"]) cfg.network.port = nodeToU16(net["port"]);
            if (auto sd = net["sd"]) {
                if (sd["multicast_address"])
                    cfg.network.sd.multicast_address =
                        sd["multicast_address"].as<std::string>();
                if (sd["multicast_port"])
                    cfg.network.sd.multicast_port =
                        nodeToU16(sd["multicast_port"]);
                if (sd["cyclic_offer_delay_ms"])
                    cfg.network.sd.cyclic_offer_delay_ms =
                        sd["cyclic_offer_delay_ms"].as<uint32_t>();
            }
        }

        if (auto services = root["services"]) {
            for (auto it = services.begin(); it != services.end(); ++it) {
                auto name = it->first.as<std::string>();
                auto node = it->second;
                ServiceDescriptor desc;
                desc.name = name;
                if (node["service_id"])
                    desc.service_id = nodeToU16(node["service_id"]);
                if (node["instance_id"])
                    desc.instance_id = nodeToU16(node["instance_id"]);

                if (auto methods = node["methods"]) {
                    for (auto m = methods.begin(); m != methods.end(); ++m) {
                        MethodConfig mc;
                        mc.name = m->first.as<std::string>();
                        mc.id = nodeToU16(m->second);
                        desc.methods[mc.name] = mc;
                    }
                }

                if (auto events = node["events"]) {
                    for (auto e = events.begin(); e != events.end(); ++e) {
                        EventConfig ec;
                        ec.name = e->first.as<std::string>();
                        ec.id = nodeToU16(e->second);
                        desc.events[ec.name] = ec;
                    }
                }

                if (auto egs = node["eventgroups"]) {
                    for (auto eg = egs.begin(); eg != egs.end(); ++eg) {
                        EventGroupConfig egc;
                        egc.name = eg->first.as<std::string>();
                        egc.id = nodeToU16(eg->second);
                        desc.eventgroups[egc.name] = egc;
                    }
                }

                if (auto fields = node["fields"]) {
                    for (auto f = fields.begin(); f != fields.end(); ++f) {
                        FieldConfig fc;
                        fc.name = f->first.as<std::string>();
                        auto fnode = f->second;
                        if (fnode["getter"])
                            fc.getter = nodeToU16(fnode["getter"]);
                        if (fnode["setter"])
                            fc.setter = nodeToU16(fnode["setter"]);
                        if (fnode["notifier"])
                            fc.notifier = nodeToU16(fnode["notifier"]);
                        desc.fields[fc.name] = fc;
                    }
                }

                cfg.services[name] = desc;
            }
        }

        applyEnvOverrides(cfg);
        return cfg;
    } catch (const YAML::Exception&) {
        return std::nullopt;
    }
}

std::optional<GatewayConfig> ConfigLoader::parseGateway(
    const std::string& yaml_content) {
    try {
        auto root = YAML::Load(yaml_content);
        GatewayConfig cfg;

        if (auto mappings = root["mappings"]) {
            for (const auto& m : mappings) {
                GatewayMapping gm;
                if (m["name"]) gm.name = m["name"].as<std::string>();

                if (m["direction"]) {
                    auto dir = m["direction"].as<std::string>();
                    gm.direction = (dir == "can_to_someip")
                                       ? MappingDirection::CanToSomeIp
                                       : MappingDirection::SomeIpToCan;
                }

                if (auto s = m["someip"]) {
                    if (s["service_id"])
                        gm.someip.service_id = nodeToU16(s["service_id"]);
                    if (s["method_id"])
                        gm.someip.method_id = nodeToU16(s["method_id"]);
                    if (s["event_id"])
                        gm.someip.event_id = nodeToU16(s["event_id"]);
                    if (s["eventgroup_id"])
                        gm.someip.eventgroup_id =
                            nodeToU16(s["eventgroup_id"]);
                }

                if (auto c = m["can"]) {
                    if (c["id"]) gm.can.id = nodeToU32(c["id"]);
                    if (c["dlc"]) gm.can.dlc = c["dlc"].as<uint8_t>();
                }

                cfg.mappings.push_back(gm);
            }
        }

        return cfg;
    } catch (const YAML::Exception&) {
        return std::nullopt;
    }
}

std::optional<ServicesConfig> ConfigLoader::loadServices(
    const std::string& file_path) {
    std::ifstream f(file_path);
    if (!f.is_open()) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseServices(ss.str());
}

std::optional<GatewayConfig> ConfigLoader::loadGateway(
    const std::string& file_path) {
    std::ifstream f(file_path);
    if (!f.is_open()) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseGateway(ss.str());
}

void ConfigLoader::applyEnvOverrides(ServicesConfig& config) {
    if (auto* val = std::getenv("BODY_ECU_NETWORK_HOST")) {
        config.network.host = val;
    }
    if (auto* val = std::getenv("BODY_ECU_NETWORK_PORT")) {
        config.network.port =
            static_cast<uint16_t>(std::stoul(std::string(val)));
    }
}

}  // namespace body_ecu::config
