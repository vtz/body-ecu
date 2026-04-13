#include <csignal>
#include <cstdio>
#include <atomic>
#include <thread>
#include <chrono>

#include "SomeIpSystem.h"
#include "autosd_adapters/SomeIpKuksaBridge.h"
#include "cloud_gateway/CloudGatewayClient.h"
#include "linux_adapters/InProcessSignalBus.h"
#include "linux_adapters/StubCloudTransport.h"

using namespace body_ecu;

static std::atomic<bool> g_running{true};

static void signalHandler(int /*sig*/) {
    g_running = false;
}

int main(int argc, char* argv[])
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::printf("=== Body ECU (POSIX MPU) ===\n");

    const char* mcu_host = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t mcu_port = 30490;
    std::printf("SOME/IP client connecting to %s:%u\n\n", mcu_host,
                mcu_port);

    adapters::SomeIpConfig someip_cfg{.host = mcu_host, .port = mcu_port};
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

    platform::CloudGatewayConfig gw_config;
    platform::CloudGatewayClient gateway(cloud_transport, signal_bus,
                                         gw_config);

    std::printf("Initializing...\n");
    someip_client.init();
    bridge.init();
    gateway.init();

    std::printf("Starting...\n");
    someip_client.run();

    std::printf("\nBody ECU MPU ready. Press Ctrl+C to shut down.\n\n");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::printf("\nShutting down...\n");
    gateway.shutdown();
    bridge.shutdown();
    someip_client.shutdown();

    std::printf("Body ECU MPU stopped.\n");
    return 0;
}
