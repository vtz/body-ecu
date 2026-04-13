#include <csignal>
#include <cstdio>
#include <atomic>
#include <thread>
#include <chrono>

#include "SomeIpSystem.h"
#include "autosd_adapters/KuksaSignalBusAdapter.h"
#include "autosd_adapters/NatsCloudTransportAdapter.h"
#include "autosd_adapters/SomeIpKuksaBridge.h"
#include "cloud_gateway/CloudGatewayClient.h"

using namespace body_ecu;

static std::atomic<bool> g_running{true};

static void signalHandler(int /*sig*/) {
    g_running = false;
}

int main(int argc, char* argv[])
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::printf("=== Body ECU (AutoSD MPU) ===\n");

    const char* mcu_host = (argc > 1) ? argv[1] : "192.168.100.10";
    std::printf("SOME/IP client -> MCU at %s:30490\n\n", mcu_host);

    adapters::SomeIpConfig someip_cfg{
        .host = mcu_host, .port = 30490, .role = adapters::SomeIpRole::Client};
    adapters::SomeIpSystem someip_client(someip_cfg);

    adapters::KuksaConfig kuksa_cfg;
    adapters::KuksaSignalBusAdapter signal_bus(kuksa_cfg);

    adapters::NatsConfig nats_cfg;
    adapters::NatsCloudTransportAdapter cloud_transport(nats_cfg);

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

    platform::CloudGatewayConfig gw_config;
    platform::CloudGatewayClient gateway(cloud_transport, signal_bus,
                                         gw_config);

    std::printf("Initializing...\n");
    signal_bus.connect();
    someip_client.init();
    bridge.init();
    gateway.init();

    std::printf("Starting...\n");
    someip_client.run();

    std::printf("\nBody ECU AutoSD MPU ready. Press Ctrl+C to shut down.\n\n");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::printf("\nShutting down...\n");
    gateway.shutdown();
    bridge.shutdown();
    someip_client.shutdown();
    signal_bus.disconnect();

    std::printf("Body ECU AutoSD MPU stopped.\n");
    return 0;
}
