#include <gtest/gtest.h>

#include <cstdlib>

#include "config_loader/ConfigLoader.h"

using namespace body_ecu::config;

static const char* kServicesYaml = R"(
network:
  host: "0.0.0.0"
  port: 30490
  sd:
    multicast_address: "239.255.255.251"
    multicast_port: 30490
    cyclic_offer_delay_ms: 5000

services:
  lighting:
    service_id: 0x1000
    instance_id: 0x0001
    methods:
      set_light_state: 0x0001
      get_light_status: 0x0002
    events:
      light_status_changed: 0x8001
    eventgroups:
      lighting_events: 0x0001

  door_lock:
    service_id: 0x1001
    instance_id: 0x0001
    methods:
      lock: 0x0001
      unlock: 0x0002
      get_status: 0x0003
    events:
      lock_state_changed: 0x8001
    eventgroups:
      door_events: 0x0001

  vehicle_mode:
    service_id: 0x1002
    instance_id: 0x0001
    fields:
      mode:
        getter: 0x0001
        setter: 0x0002
        notifier: 0x8001
    eventgroups:
      mode_events: 0x0001
)";

static const char* kGatewayYaml = R"(
mappings:
  - name: "light_command"
    direction: someip_to_can
    someip:
      service_id: 0x1000
      method_id: 0x0001
    can:
      id: 0x200
      dlc: 4

  - name: "door_status"
    direction: can_to_someip
    can:
      id: 0x300
      dlc: 2
    someip:
      service_id: 0x1001
      event_id: 0x8001
      eventgroup_id: 0x0001
)";

TEST(ConfigLoaderTest, YamlParsing) {
    auto cfg = ConfigLoader::parseServices(kServicesYaml);
    ASSERT_TRUE(cfg.has_value());

    EXPECT_EQ(cfg->network.host, "0.0.0.0");
    EXPECT_EQ(cfg->network.port, 30490);
    EXPECT_EQ(cfg->network.sd.multicast_address, "239.255.255.251");

    ASSERT_EQ(cfg->services.size(), 3u);
    ASSERT_TRUE(cfg->services.count("lighting"));
    ASSERT_TRUE(cfg->services.count("door_lock"));
    ASSERT_TRUE(cfg->services.count("vehicle_mode"));

    const auto& lighting = cfg->services.at("lighting");
    EXPECT_EQ(lighting.service_id, 0x1000);
    EXPECT_EQ(lighting.instance_id, 0x0001);
    EXPECT_EQ(lighting.methods.at("set_light_state").id, 0x0001);
    EXPECT_EQ(lighting.methods.at("get_light_status").id, 0x0002);
    EXPECT_EQ(lighting.events.at("light_status_changed").id, 0x8001);
    EXPECT_EQ(lighting.eventgroups.at("lighting_events").id, 0x0001);

    const auto& door = cfg->services.at("door_lock");
    EXPECT_EQ(door.service_id, 0x1001);
    EXPECT_EQ(door.methods.size(), 3u);
}

TEST(ConfigLoaderTest, HexParsing) {
    EXPECT_EQ(ConfigLoader::parseHex16("0x1000"), 0x1000);
    EXPECT_EQ(ConfigLoader::parseHex16("0x8001"), 0x8001);
    EXPECT_EQ(ConfigLoader::parseHex16("0x0001"), 0x0001);
    EXPECT_EQ(ConfigLoader::parseHex32("0x200"), 0x200u);
    EXPECT_EQ(ConfigLoader::parseHex32("0x300"), 0x300u);
}

TEST(ConfigLoaderTest, EnvOverride) {
    setenv("BODY_ECU_NETWORK_PORT", "30491", 1);
    auto cfg = ConfigLoader::parseServices(kServicesYaml);
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->network.port, 30491);
    unsetenv("BODY_ECU_NETWORK_PORT");
}

TEST(ConfigLoaderTest, MissingKey) {
    auto cfg = ConfigLoader::parseServices("network:\n  host: '127.0.0.1'\n");
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->services.size(), 0u);
    EXPECT_EQ(cfg->network.host, "127.0.0.1");
    EXPECT_EQ(cfg->network.port, 30490);
}

TEST(ConfigLoaderTest, MalformedYaml) {
    auto cfg = ConfigLoader::parseServices("{{{{not yaml at all::::");
    EXPECT_FALSE(cfg.has_value());
}

TEST(ConfigLoaderTest, CanGatewayConfig) {
    auto cfg = ConfigLoader::parseGateway(kGatewayYaml);
    ASSERT_TRUE(cfg.has_value());
    ASSERT_EQ(cfg->mappings.size(), 2u);

    const auto& light_cmd = cfg->mappings[0];
    EXPECT_EQ(light_cmd.name, "light_command");
    EXPECT_EQ(light_cmd.direction, MappingDirection::SomeIpToCan);
    EXPECT_EQ(light_cmd.someip.service_id, 0x1000);
    EXPECT_EQ(light_cmd.someip.method_id, 0x0001);
    EXPECT_EQ(light_cmd.can.id, 0x200u);
    EXPECT_EQ(light_cmd.can.dlc, 4);

    const auto& door_status = cfg->mappings[1];
    EXPECT_EQ(door_status.name, "door_status");
    EXPECT_EQ(door_status.direction, MappingDirection::CanToSomeIp);
    EXPECT_EQ(door_status.can.id, 0x300u);
    EXPECT_EQ(door_status.can.dlc, 2);
    EXPECT_EQ(door_status.someip.service_id, 0x1001);
    EXPECT_EQ(door_status.someip.event_id, 0x8001);
    EXPECT_EQ(door_status.someip.eventgroup_id, 0x0001);
}

TEST(ConfigLoaderTest, VehicleModeFieldsParsed) {
    auto cfg = ConfigLoader::parseServices(kServicesYaml);
    ASSERT_TRUE(cfg.has_value());

    const auto& mode = cfg->services.at("vehicle_mode");
    EXPECT_EQ(mode.service_id, 0x1002);
    ASSERT_TRUE(mode.fields.count("mode"));
    EXPECT_EQ(mode.fields.at("mode").getter, 0x0001);
    EXPECT_EQ(mode.fields.at("mode").setter, 0x0002);
    EXPECT_EQ(mode.fields.at("mode").notifier, 0x8001);
}
