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
#include "linux_adapters/InProcessSignalBus.h"
#include "linux_adapters/StubCloudTransport.h"

#include "cli.h"

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

    adapters::InProcessSignalBus signal_bus;
    adapters::StubCloudTransport cloud_transport;

    adapters::SomeIpKuksaBridge bridge(someip_client, signal_bus);

    adapters::BridgeMapping lock_event;
    lock_event.signal_path =
        "Vehicle.Cabin.Door.Row1.DriverSide.IsLocked";
    lock_event.direction = adapters::BridgeDirection::EventToSignal;
    lock_event.someip_service_id = 0x1001;
    lock_event.someip_method_or_event_id = 0x8001;
    lock_event.someip_eventgroup_id = 0x0001;
    bridge.addMapping(lock_event);

    adapters::BridgeMapping lock_cmd;
    lock_cmd.signal_path = "Vehicle.Command.Door.Lock";
    lock_cmd.direction = adapters::BridgeDirection::SignalToMethod;
    lock_cmd.someip_service_id = 0x1001;
    lock_cmd.someip_method_or_event_id = 0x0001;
    bridge.addMapping(lock_cmd);

    adapters::BridgeMapping speed_event;
    speed_event.signal_path = "Vehicle.Speed";
    speed_event.direction = adapters::BridgeDirection::EventToSignal;
    speed_event.someip_service_id = 0x1003;
    speed_event.someip_method_or_event_id = 0x8001;
    speed_event.someip_eventgroup_id = 0x0001;
    bridge.addMapping(speed_event);

    adapters::BridgeMapping mode_event;
    mode_event.signal_path = "Vehicle.Mode";
    mode_event.direction = adapters::BridgeDirection::EventToSignal;
    mode_event.someip_service_id = 0x1002;
    mode_event.someip_method_or_event_id = 0x8001;
    mode_event.someip_eventgroup_id = 0x0001;
    bridge.addMapping(mode_event);

    adapters::BridgeMapping light_event;
    light_event.signal_path = "Vehicle.Lights.Status";
    light_event.direction = adapters::BridgeDirection::EventToSignal;
    light_event.someip_service_id = 0x1000;
    light_event.someip_method_or_event_id = 0x8001;
    light_event.someip_eventgroup_id = 0x0001;
    bridge.addMapping(light_event);

    platform::CloudGatewayConfig gw_config;
    platform::CloudGatewayClient gateway(cloud_transport, signal_bus,
                                         gw_config);

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

    std::printf("Body ECU MPU stopped.\n");
    return 0;
}
