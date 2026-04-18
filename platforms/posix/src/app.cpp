#include "door_lock/DoorLockController.h"
#include "CanGatewaySystem.h"
#include "DiagnosticsSystem.h"
#include "DoCanTransport.h"
#include "DoIpTransport.h"
#include "DoorLockSystem.h"
#include "SpeedSimulatorSystem.h"
#include "VehicleInfoProvider.h"
#include "LightingSystem.h"
#include "SomeIpSystem.h"
#include "VehicleModeSystem.h"

#include "linux_adapters/ConsoleGpioAdapter.h"
#include "linux_adapters/InProcessSignalBus.h"
#include "linux_adapters/SimulatedAdcAdapter.h"
#include "linux_adapters/SocketCanAdapter.h"
#include "linux_adapters/StdinButtonAdapter.h"
#include "linux_adapters/ThreadTimerService.h"

#include <async/AsyncBinding.h>
#include <lifecycle/LifecycleManager.h>

#include <bsp/timer/SystemTimer.h>

#include <cstdio>

using namespace body_ecu;

using AsyncAdapter        = ::async::AsyncBinding::AdapterType;
using AsyncRuntimeMonitor = ::async::AsyncBinding::RuntimeMonitorType;
using AsyncContextHook    = ::async::AsyncBinding::ContextHookType;

static constexpr size_t MaxNumComponents         = 16;
static constexpr size_t MaxNumLevels             = 8;
static constexpr size_t MaxNumComponentsPerLevel = MaxNumComponents;

using LifecycleManager = ::lifecycle::declare::
    LifecycleManager<MaxNumComponents, MaxNumLevels, MaxNumComponentsPerLevel>;

static char const* const isrGroupNames[ISR_GROUP_COUNT] = {"test"};

static AsyncRuntimeMonitor runtimeMonitor{
    AsyncContextHook::InstanceType::GetNameType::create<&AsyncAdapter::getTaskName>(),
    isrGroupNames};

static LifecycleManager lifecycleManager{
    TASK_SYSADMIN,
    ::lifecycle::LifecycleManager::GetTimestampType::create<&getSystemTimeUs32Bit>()};

void app_main()
{
    std::printf("=== Body ECU (POSIX + OpenBSW) ===\n");
    std::printf("Using OpenBSW lifecycle, async framework\n\n");

    adapters::SomeIpConfig someip_cfg{.host = "0.0.0.0", .port = 30490};
    static adapters::SomeIpSystem someip_system(someip_cfg);

    static adapters::ConsoleGpioAdapter gpio_adapter(
        {"HEADLIGHT (green)", "TURN_SIGNAL (yellow)", "BRAKE (red)"});

    static adapters::SocketCanAdapter can_adapter("vcan0");
    if (!can_adapter.open()) {
        std::printf("[WARN] CAN interface 'vcan0' not available\n");
    }

    static adapters::StdinButtonAdapter button_adapter;
    static adapters::InProcessSignalBus signal_bus;

    static adapters::LightingSystem lighting(gpio_adapter, someip_system);
    static adapters::DoorLockSystem door_lock(gpio_adapter, button_adapter,
                                              someip_system,
                                              body::DoorLockConfig{}, &signal_bus);
    static adapters::VehicleModeSystem vehicle_mode(someip_system);

    static adapters::SimulatedAdcAdapter adc_adapter;
    static adapters::ThreadTimerService timer_service;
    static adapters::SpeedSimulatorSystem speed_sim(
        adc_adapter, someip_system, timer_service,
        body::SpeedSimulatorConfig{}, &signal_bus);

    static adapters::CanGatewaySystem can_gateway(can_adapter, someip_system);

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

    static adapters::VehicleInfoProvider vehicle_info;
    vehicle_info.setVin("WVW00000BODYECU01");
    vehicle_info.setEcuSerial("BECU-001");

    static adapters::DiagnosticsSystem diagnostics;
    static adapters::DoIpTransport doip_transport;
    static adapters::DoCanTransport docan_transport(can_adapter);
    diagnostics.addTransport(&doip_transport);
    diagnostics.addTransport(&docan_transport);
    diagnostics.addProvider(&vehicle_info);
    diagnostics.addProvider(&lighting.controller());
    diagnostics.addProvider(&door_lock.controller());

    vehicle_mode.manager().addObserver(&lighting.controller());
    vehicle_mode.manager().addObserver(&door_lock.controller());

    lifecycleManager.addComponent("someip",       someip_system,    1);
    lifecycleManager.addComponent("lighting",     lighting,         2);
    lifecycleManager.addComponent("door_lock",    door_lock,        2);
    lifecycleManager.addComponent("vehicle_mode", vehicle_mode,     2);
    lifecycleManager.addComponent("speed_sim",    speed_sim,        2);
    lifecycleManager.addComponent("can_gateway",  can_gateway,      3);
    lifecycleManager.addComponent("diagnostics",  diagnostics,      3);
    lifecycleManager.addComponent("doip",         doip_transport,   3);
    lifecycleManager.addComponent("docan",        docan_transport,  3);

    lifecycleManager.transitionToLevel(MaxNumLevels);

    runtimeMonitor.start();
    AsyncAdapter::run();

    while (true)
    {
        ;
    }
}

void idle(AsyncAdapter::TaskContextType& taskContext)
{
    taskContext.dispatchWhileWork();
}

using IdleTask = AsyncAdapter::IdleTask<1024 * 2>;
IdleTask idleTask{"idle", AsyncAdapter::TaskFunctionType::create<&idle>()};

using TimerTask = AsyncAdapter::TimerTask<1024 * 1>;
TimerTask timerTask{"timer"};

using BodyTask = AsyncAdapter::Task<TASK_BODY, 1024 * 2>;
BodyTask bodyTask{"body"};

using SomeIpTask = AsyncAdapter::Task<TASK_SOMEIP, 1024 * 2>;
SomeIpTask someipTask{"someip"};

using DiagTask = AsyncAdapter::Task<TASK_DIAG, 1024 * 2>;
DiagTask diagTask{"diag"};

using BackgroundTask = AsyncAdapter::Task<TASK_BACKGROUND, 1024 * 2>;
BackgroundTask backgroundTask{"background"};

using SysadminTask = AsyncAdapter::Task<TASK_SYSADMIN, 1024 * 2>;
SysadminTask sysadminTask{"sysadmin"};

AsyncContextHook contextHook{runtimeMonitor};
