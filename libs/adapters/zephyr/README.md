# zephyr -- Zephyr Platform Adapters

Implements port interfaces using the Zephyr RTOS driver APIs. These adapters are only compiled when building for a Zephyr target (excluded in host-based test builds via `BUILD_TESTS` guard).

## Adapters

| Adapter | Port Interface | Zephyr API |
|---------|---------------|------------|
| `GpioAdapter` | `IGpioPort` | `gpio_pin_configure`, `gpio_pin_set`, `gpio_pin_get` |
| `CanAdapter` | `ICanBus` | `can_send`, `can_add_rx_filter`, CAN-FD support |
| `ButtonAdapter` | `IButtonInput` | `gpio_pin_interrupt_configure`, GPIO callback |

## Hardware Mapping (NUCLEO-H755ZI-Q)

- **LEDs**: LD1 (green, PB0), LD2 (yellow, PE1), LD3 (red, PB14) via `led0`/`led1`/`led2` aliases
- **User Button**: B1 (PC13) via `sw0` alias
- **CAN-FD**: FDCAN1 on PD0/PD1
- **Ethernet**: RMII via ETH PHY

## HPC Migration

For migration to Linux HPC, replace these adapters with:
- `GpioAdapter` -> Linux sysfs/gpiod
- `CanAdapter` -> Linux SocketCAN
- `ButtonAdapter` -> Linux input event subsystem
