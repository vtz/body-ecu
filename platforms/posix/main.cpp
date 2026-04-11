#include <csignal>
#include <cstdio>
#include <atomic>
#include <thread>
#include <chrono>

#include "CanGatewaySystem.h"
#include "DiagnosticsSystem.h"
#include "DoCanTransport.h"
#include "DoIpTransport.h"
#include "DoorLockSystem.h"
#include "LightingSystem.h"
#include "SomeIpSystem.h"
#include "VehicleModeSystem.h"

#include "linux_adapters/ConsoleGpioAdapter.h"
#include "linux_adapters/SocketCanAdapter.h"
#include "linux_adapters/StdinButtonAdapter.h"

using namespace body_ecu;

static std::atomic<bool> g_running{true};

static void signalHandler(int /*sig*/) {
    g_running = false;
}

int main(int argc, char* argv[])
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::printf("=== Body ECU (POSIX) ===\n");
    std::printf("Platform: Linux/POSIX\n\n");

    const char* can_iface = (argc > 1) ? argv[1] : "vcan0";

    // --- SOME/IP transport ---
    adapters::SomeIpConfig someip_cfg{.host = "0.0.0.0", .port = 30490};
    adapters::SomeIpSystem someip_system(someip_cfg);

    // --- Linux platform adapters ---
    adapters::ConsoleGpioAdapter gpio_adapter(
        {"HEADLIGHT (green)", "TURN_SIGNAL (yellow)", "BRAKE (red)"});

    adapters::SocketCanAdapter can_adapter(can_iface);
    if (!can_adapter.open()) {
        std::printf("[WARN] CAN interface '%s' not available, "
                    "CAN gateway will be non-functional\n", can_iface);
    }

    adapters::StdinButtonAdapter button_adapter;

    // --- Lifecycle systems ---
    adapters::LightingSystem lighting(gpio_adapter, someip_system);
    adapters::DoorLockSystem door_lock(gpio_adapter, button_adapter,
                                       someip_system);
    adapters::VehicleModeSystem vehicle_mode(someip_system);

    adapters::CanGatewaySystem can_gateway(can_adapter, someip_system);

    platform::ServiceMapping light_gw;
    light_gw.name = "light_command";
    light_gw.direction = platform::GatewayDirection::SomeIpToCan;
    light_gw.someip_service_id = 0x1000;
    light_gw.someip_method_id = 0x0001;
    light_gw.can_id = 0x200;
    light_gw.can_dlc = 4;
    can_gateway.addMapping(light_gw);

    platform::ServiceMapping door_gw;
    door_gw.name = "door_status";
    door_gw.direction = platform::GatewayDirection::CanToSomeIp;
    door_gw.someip_service_id = 0x1001;
    door_gw.someip_event_id = 0x8001;
    door_gw.someip_eventgroup_id = 0x0001;
    door_gw.can_id = 0x300;
    door_gw.can_dlc = 2;
    can_gateway.addMapping(door_gw);

    // --- Diagnostics (dual transport: DoIP + DoCAN) ---
    adapters::DiagnosticsSystem diagnostics;
    adapters::DoIpTransport doip_transport;
    adapters::DoCanTransport docan_transport(can_adapter);
    diagnostics.addTransport(&doip_transport);
    diagnostics.addTransport(&docan_transport);
    diagnostics.addProvider(&lighting.controller());
    diagnostics.addProvider(&door_lock.controller());

    // --- Cross-service observers ---
    vehicle_mode.manager().addObserver(&lighting.controller());
    vehicle_mode.manager().addObserver(&door_lock.controller());

    // --- Init ---
    std::printf("Initializing systems...\n");
    someip_system.init();
    lighting.init();
    door_lock.init();
    vehicle_mode.init();
    can_gateway.init();
    diagnostics.init();
    doip_transport.init();
    docan_transport.init();

    // --- Run ---
    std::printf("Starting systems...\n");
    someip_system.run();
    lighting.run();
    door_lock.run();
    vehicle_mode.run();
    can_gateway.run();
    diagnostics.run();
    button_adapter.start();

    std::printf("\nBody ECU ready. Press Ctrl+C to shut down.\n");
    std::printf("Press Enter to toggle door lock.\n\n");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // --- Shutdown ---
    std::printf("\nShutting down...\n");
    button_adapter.stop();
    can_gateway.shutdown();
    diagnostics.shutdown();
    doip_transport.shutdown();
    docan_transport.shutdown();
    lighting.shutdown();
    door_lock.shutdown();
    vehicle_mode.shutdown();
    someip_system.shutdown();
    can_adapter.close();

    std::printf("Body ECU stopped.\n");
    return 0;
}
