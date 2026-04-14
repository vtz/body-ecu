#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <optional>

LOG_MODULE_REGISTER(body_ecu, LOG_LEVEL_INF);

#ifdef CONFIG_GPIO
#include <zephyr/drivers/gpio.h>
#endif

#ifdef CONFIG_CAN
#include <zephyr/drivers/can.h>
#endif

#include "lifecycle/LifecycleManager.h"
#include "CanGatewaySystem.h"
#include "DiagnosticsSystem.h"
#include "DoCanTransport.h"
#include "DoIpTransport.h"
#include "DoorLockSystem.h"
#include "LightingSystem.h"
#include "SomeIpSystem.h"
#include "VehicleModeSystem.h"

#ifdef CONFIG_GPIO
#include "zephyr_adapters/ButtonAdapter.h"
#include "zephyr_adapters/CanAdapter.h"
#include "zephyr_adapters/GpioAdapter.h"
#endif

#include "zephyr_adapters/LocalSignalBus.h"

using namespace body_ecu;

int main(void)
{
    LOG_INF("Body ECU starting");
    LOG_INF("Platform: %s", CONFIG_BOARD);

    adapters::SomeIpConfig someip_cfg{.host = "0.0.0.0", .port = 30490};
    adapters::SomeIpSystem someip_system(someip_cfg);

#ifdef CONFIG_GPIO
    static const struct gpio_dt_spec leds[] = {
        GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0}),
        GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0}),
        GPIO_DT_SPEC_GET_OR(DT_ALIAS(led2), gpios, {0}),
    };

    std::vector<adapters::GpioAdapter::PinMapping> pin_map;
    for (const auto& led : leds) {
        pin_map.push_back({led.port, led.pin, GPIO_ACTIVE_HIGH});
    }
    adapters::GpioAdapter gpio_adapter(pin_map);
    bool gpio_ok = gpio_adapter.configure();

    static const struct gpio_dt_spec user_btn =
        GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});
    adapters::ButtonAdapter button_adapter(user_btn.port, user_btn.pin);
    if (gpio_ok) {
        button_adapter.configure();
    }
    if (!gpio_ok) {
        LOG_WRN("GPIO not available -- lighting/door-lock disabled (emulation?)");
    }
#endif

#ifdef CONFIG_CAN
    const struct device* can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
    adapters::CanAdapter can_adapter(can_dev);
    can_adapter.configure();
#endif

    adapters::LocalSignalBus signal_bus;

#ifdef CONFIG_GPIO
    std::optional<adapters::LightingSystem> lighting;
    std::optional<adapters::DoorLockSystem> door_lock;
    if (gpio_ok) {
        lighting.emplace(gpio_adapter, someip_system);
        door_lock.emplace(gpio_adapter, button_adapter, someip_system,
                          body::DoorLockConfig{}, &signal_bus);
    }
#endif
    adapters::VehicleModeSystem vehicle_mode(someip_system);

#ifdef CONFIG_CAN
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
#endif

    adapters::DiagnosticsSystem diagnostics;
    adapters::DoIpTransport doip_transport;
#ifdef CONFIG_CAN
    adapters::DoCanTransport docan_transport(can_adapter);
    diagnostics.addTransport(&docan_transport);
#endif
    diagnostics.addTransport(&doip_transport);

#ifdef CONFIG_GPIO
    if (lighting && door_lock) {
        diagnostics.addProvider(&lighting->controller());
        diagnostics.addProvider(&door_lock->controller());
        vehicle_mode.manager().addObserver(&lighting->controller());
        vehicle_mode.manager().addObserver(&door_lock->controller());
    }
#endif

    lifecycle::LifecycleManager lm;
    lm.addComponent("someip", someip_system, 1);
#ifdef CONFIG_GPIO
    if (lighting) lm.addComponent("lighting", *lighting, 2);
    if (door_lock) lm.addComponent("door_lock", *door_lock, 2);
#endif
    lm.addComponent("vehicle_mode", vehicle_mode, 2);
#ifdef CONFIG_CAN
    lm.addComponent("can_gateway", can_gateway, 3);
    lm.addComponent("docan", docan_transport, 3);
#endif
    lm.addComponent("diagnostics", diagnostics, 3);
    lm.addComponent("doip", doip_transport, 3);

    LOG_INF("Transitioning to run level 3...");
    lm.transitionToLevel(3);
    LOG_INF("Body ECU ready - all systems running");

    k_sleep(K_FOREVER);
    return 0;
}
