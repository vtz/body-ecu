#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>

#include "lifecycle/LifecycleManager.h"
#include "SomeIpSystem.h"
#include "autosd_adapters/SomeIpKuksaBridge.h"
#include "cloud_gateway/CloudGatewayClient.h"

#ifdef BODY_ECU_REAL_ADAPTERS
#include "autosd_adapters/KuksaSignalBusAdapter.h"
#include "autosd_adapters/NatsCloudTransportAdapter.h"
#else
#include "linux_adapters/InProcessSignalBus.h"
#include "linux_adapters/StubCloudTransport.h"
#endif

#include "cli.h"
#include "someip_service_ids.h"

using namespace body_ecu;

static std::atomic<bool> g_running{true};

static void signalHandler(int /*sig*/) {
    g_running = false;
}

int main(int argc, char* argv[])
{
    std::setbuf(stdout, nullptr);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::printf("=== Body ECU (POSIX MPU) ===\n");

    const char* mcu_host = (argc > 1) ? argv[1]
                           : std::getenv("MCU_HOST") ? std::getenv("MCU_HOST")
                           : "127.0.0.1";
    uint16_t mcu_port = 30490;
    std::printf("SOME/IP client connecting to %s:%u\n\n", mcu_host,
                mcu_port);

    adapters::SomeIpConfig someip_cfg{
        .host = mcu_host, .port = mcu_port, .role = adapters::SomeIpRole::Client};
    adapters::SomeIpSystem someip_client(someip_cfg);

#ifdef BODY_ECU_REAL_ADAPTERS
    const char* kuksa_host = std::getenv("KUKSA_HOST") ? std::getenv("KUKSA_HOST") : "localhost";
    uint16_t kuksa_port = std::getenv("KUKSA_PORT") ? static_cast<uint16_t>(std::atoi(std::getenv("KUKSA_PORT"))) : 55555;
    adapters::KuksaConfig kuksa_cfg{kuksa_host, kuksa_port};
    adapters::KuksaSignalBusAdapter signal_bus(kuksa_cfg);

    const char* nats_url_env = std::getenv("NATS_URL");
    adapters::NatsConfig nats_cfg{nats_url_env ? nats_url_env : "nats://localhost:4222"};
    adapters::NatsCloudTransportAdapter cloud_transport(nats_cfg);

    std::printf("Real adapters: Kuksa=%s:%u, NATS=%s\n",
                kuksa_cfg.host.c_str(), kuksa_cfg.port, nats_cfg.url.c_str());
#else
    adapters::InProcessSignalBus signal_bus;
    adapters::StubCloudTransport cloud_transport;
    std::printf("Using stub adapters (build with -DBODY_ECU_REAL_ADAPTERS=ON for real NATS/Kuksa)\n");
#endif

    adapters::SomeIpKuksaBridge bridge(someip_client, signal_bus);

    adapters::BridgeMapping lock_event;
    lock_event.signal_path =
        "Vehicle.Cabin.Door.Row1.DriverSide.IsLocked";
    lock_event.direction = adapters::BridgeDirection::EventToSignal;
    lock_event.datatype = adapters::SignalDataType::Bool;
    lock_event.someip_service_id = body_ecu::someip::door_lock::kServiceId;
    lock_event.someip_method_or_event_id = body_ecu::someip::door_lock::event::kLockStateChanged;
    lock_event.someip_eventgroup_id = body_ecu::someip::door_lock::eventgroup::kDoorEvents;
    bridge.addMapping(lock_event);

    adapters::BridgeMapping lock_cmd;
    lock_cmd.signal_path = "Vehicle.Command.Door.Lock";
    lock_cmd.direction = adapters::BridgeDirection::SignalToMethod;
    lock_cmd.datatype = adapters::SignalDataType::Bool;
    lock_cmd.someip_service_id = body_ecu::someip::door_lock::kServiceId;
    lock_cmd.someip_method_or_event_id = body_ecu::someip::door_lock::method::kLock;
    lock_cmd.someip_false_method_id = body_ecu::someip::door_lock::method::kUnlock;
    bridge.addMapping(lock_cmd);

    adapters::BridgeMapping speed_event;
    speed_event.signal_path = "Vehicle.Speed";
    speed_event.direction = adapters::BridgeDirection::EventToSignal;
    speed_event.datatype = adapters::SignalDataType::Float;
    speed_event.someip_service_id = body_ecu::someip::speed_sensor::kServiceId;
    speed_event.someip_method_or_event_id = body_ecu::someip::speed_sensor::event::kSpeedChanged;
    speed_event.someip_eventgroup_id = body_ecu::someip::speed_sensor::eventgroup::kSpeedEvents;
    bridge.addMapping(speed_event);

    adapters::BridgeMapping mode_event;
    mode_event.signal_path = "Vehicle.Mode";
    mode_event.direction = adapters::BridgeDirection::EventToSignal;
    mode_event.someip_service_id = body_ecu::someip::vehicle_mode::kServiceId;
    mode_event.someip_method_or_event_id = body_ecu::someip::vehicle_mode::field::kModeNotifier;
    mode_event.someip_eventgroup_id = body_ecu::someip::vehicle_mode::eventgroup::kModeEvents;
    bridge.addMapping(mode_event);

    adapters::BridgeMapping light_event;
    light_event.signal_path = "Vehicle.Lights.Status";
    light_event.direction = adapters::BridgeDirection::EventToSignal;
    light_event.someip_service_id = body_ecu::someip::lighting::kServiceId;
    light_event.someip_method_or_event_id = body_ecu::someip::lighting::event::kLightStatusChanged;
    light_event.someip_eventgroup_id = body_ecu::someip::lighting::eventgroup::kLightingEvents;
    bridge.addMapping(light_event);

    platform::CloudGatewayConfig gw_config;
    platform::CloudGatewayClient gateway(cloud_transport, signal_bus,
                                         gw_config);

#ifdef BODY_ECU_REAL_ADAPTERS
    signal_bus.connect();
#endif

    lifecycle::LifecycleManager lm;
    lm.addComponent("someip_client", someip_client, 1);

    lm.transitionToLevel(1);

    bridge.init();
    gateway.init();

    Cli cli(someip_client);
    cli.init();

    std::printf("\nBody ECU MPU ready. Type 'help' for available commands.\n\n");

    cli.start();

    while (g_running && cli.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::printf("\nShutting down...\n");
    cli.stop();
    gateway.shutdown();
    bridge.shutdown();
    lm.shutdownAll();
#ifdef BODY_ECU_REAL_ADAPTERS
    signal_bus.disconnect();
#endif

    std::printf("Body ECU MPU stopped.\n");
    return 0;
}
