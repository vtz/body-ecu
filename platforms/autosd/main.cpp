#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <chrono>

#include "lifecycle/LifecycleManager.h"
#include "SomeIpSystem.h"
#include "autosd_adapters/KuksaSignalBusAdapter.h"
#include "autosd_adapters/NatsCloudTransportAdapter.h"
#include "autosd_adapters/SomeIpKuksaBridge.h"
#include "autosd_adapters/SystemdLifecycleAdapter.h"
#include "cloud_gateway/CloudGatewayClient.h"
#include "someip_service_ids.h"
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

    std::printf("=== Body ECU (AutoSD MPU) ===\n");

    const char* mcu_host = (argc > 1) ? argv[1] : "192.168.100.10";
    std::printf("SOME/IP client -> MCU at %s:30490\n\n", mcu_host);

    adapters::SomeIpConfig someip_cfg{
        .host = mcu_host, .port = 30490, .role = adapters::SomeIpRole::Client};
    adapters::SomeIpSystem someip_client(someip_cfg);

    const char* kuksa_host = std::getenv("KUKSA_HOST") ? std::getenv("KUKSA_HOST") : "localhost";
    uint16_t kuksa_port = std::getenv("KUKSA_PORT")
        ? static_cast<uint16_t>(std::atoi(std::getenv("KUKSA_PORT"))) : 55555;
    adapters::KuksaConfig kuksa_cfg{kuksa_host, kuksa_port};
    adapters::KuksaSignalBusAdapter signal_bus(kuksa_cfg);

    const char* nats_url_env = std::getenv("NATS_URL");
    adapters::NatsConfig nats_cfg{nats_url_env ? nats_url_env : "nats://localhost:4222"};
    adapters::NatsCloudTransportAdapter cloud_transport(nats_cfg);

    adapters::SomeIpKuksaBridge bridge(someip_client, signal_bus);

    adapters::BridgeMapping lock_event;
    lock_event.signal_path = "Vehicle.Cabin.Door.Row1.DriverSide.IsLocked";
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
    light_event.datatype = adapters::SignalDataType::Bitmask;
    light_event.someip_service_id = body_ecu::someip::lighting::kServiceId;
    light_event.someip_method_or_event_id = body_ecu::someip::lighting::event::kLightStatusChanged;
    light_event.someip_eventgroup_id = body_ecu::someip::lighting::eventgroup::kLightingEvents;
    bridge.addMapping(light_event);

    adapters::BridgeMapping light_cmd;
    light_cmd.signal_path = "Vehicle.Command.Lights.Set";
    light_cmd.direction = adapters::BridgeDirection::SignalToMethod;
    light_cmd.datatype = adapters::SignalDataType::Packed2Bytes;
    light_cmd.someip_service_id = body_ecu::someip::lighting::kServiceId;
    light_cmd.someip_method_or_event_id = body_ecu::someip::lighting::method::kSetLightState;
    bridge.addMapping(light_cmd);

    adapters::BridgeMapping vin_event;
    vin_event.signal_path = "Vehicle.VIN";
    vin_event.direction = adapters::BridgeDirection::EventToSignal;
    vin_event.datatype = adapters::SignalDataType::String;
    vin_event.someip_service_id = body_ecu::someip::vehicle_info::kServiceId;
    vin_event.someip_method_or_event_id = body_ecu::someip::vehicle_info::event::kVinAvailable;
    vin_event.someip_eventgroup_id = body_ecu::someip::vehicle_info::eventgroup::kVehicleInfoEvents;
    bridge.addMapping(vin_event);

    signal_bus.connect();

    lifecycle::LifecycleManager lm;
    lm.addComponent("someip_client", someip_client, 1);

    lm.transitionToLevel(1);

    bridge.init();

    Cli cli(someip_client);
    cli.init();
    cli.registerResponseHandler(body_ecu::someip::vehicle_info::kServiceId,
                                body_ecu::someip::vehicle_info::method::kGetVin);

    std::string vin = "UNKNOWN";
    for (int attempt = 0; attempt < 5; ++attempt) {
        auto vin_resp = cli.sendRequest(
            body_ecu::someip::vehicle_info::kServiceId,
            body_ecu::someip::vehicle_info::method::kGetVin);
        if (vin_resp.return_code == 0 && !vin_resp.payload.empty()) {
            vin.assign(vin_resp.payload.begin(), vin_resp.payload.end());
            vin.erase(vin.find_last_not_of('\0') + 1);
            std::printf("[VIN] Received from MCU: %s (attempt %d)\n",
                        vin.c_str(), attempt + 1);
            break;
        }
        std::printf("[VIN] MCU not ready, retrying (%d/5)...\n", attempt + 1);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    platform::CloudGatewayConfig gw_config;
    gw_config.vin = vin;
    platform::CloudGatewayClient gateway(cloud_transport, signal_bus,
                                         gw_config);
    gateway.init();

    {
        auto subject = "vehicles." + vin + ".info.vin";
        std::vector<uint8_t> vin_bytes(vin.begin(), vin.end());
        cloud_transport.publish(subject, vin_bytes);
    }

    gateway.publishCurrentState();

    signal_bus.subscribe("Vehicle.VIN",
        [&gateway, &cloud_transport](const std::string& /*path*/,
                                     const ports::SignalValue& value) {
            auto* s = std::get_if<std::string>(&value);
            if (s && !s->empty()) {
                std::printf("[VIN] MCU published VIN via event: %s\n", s->c_str());
                gateway.updateVin(*s);
            }
        });

    adapters::SystemdLifecycleAdapter::notifyReady();
    std::printf("\nBody ECU AutoSD MPU ready. Type 'help' for available commands.\n\n");

    cli.start();

    while (g_running && cli.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    adapters::SystemdLifecycleAdapter::notifyStopping();
    std::printf("\nShutting down...\n");
    cli.stop();
    gateway.shutdown();
    bridge.shutdown();
    lm.shutdownAll();
    signal_bus.disconnect();

    std::printf("Body ECU AutoSD MPU stopped.\n");
    return 0;
}
