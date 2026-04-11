# linux -- Linux/POSIX Platform Adapters

Implements port interfaces for Linux/POSIX desktop builds, enabling full Body ECU functionality without Zephyr or embedded hardware.

## Adapters

| Adapter | Port Interface | Backend |
|---------|---------------|---------|
| `ConsoleGpioAdapter` | `IGpioPort` | Console stdout (logs pin state changes) |
| `SocketCanAdapter` | `ICanBus` | Linux SocketCAN (`AF_CAN` socket) |
| `StdinButtonAdapter` | `IButtonInput` | stdin (Enter key simulates button press) |

## SocketCAN Setup

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

Then use `candump vcan0` and `cansend vcan0` to inspect traffic.

## HPC Migration

These adapters serve as the starting point for HPC (MPU) deployment:
- `ConsoleGpioAdapter` -> replace with Linux sysfs/gpiod for real GPIO
- `SocketCanAdapter` -> already uses real SocketCAN (production-ready)
- `StdinButtonAdapter` -> replace with Linux input event subsystem (`/dev/input/eventN`)
