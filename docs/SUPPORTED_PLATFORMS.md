# Supported Platforms

## Build Targets

| Platform | Build Command | RTOS / OS | Notes |
|----------|--------------|-----------|-------|
| **POSIX (Linux/macOS)** | `cmake -B build/posix -S platforms/posix` | None (pthreads) | Real sockets, SocketCAN, console GPIO |
| native_sim | `west build -b native_sim app` | Zephyr (POSIX) | Zephyr kernel simulation |
| NUCLEO-H755ZI-Q | `west build -b nucleo_h755zi_q/stm32h755xx/m7 app` | Zephyr | M7 core, Ethernet + CAN-FD |

## Platform Adapters

| Platform | Adapter Set | GPIO | CAN | Button |
|----------|------------|------|-----|--------|
| POSIX | `libs/adapters/linux/` | Console stdout | SocketCAN (`vcan0`) | stdin (Enter key) |
| Zephyr | `libs/adapters/zephyr/` | Zephyr GPIO driver | Zephyr CAN driver | Zephyr GPIO interrupt |

Both platforms share the same domain logic (`libs/body/`), platform modules (`libs/platform/`),
and OpenBSW lifecycle wrappers (`libs/adapters/openbsw/`).

## Emulation

| Platform | Tool | Script | Notes |
|----------|------|--------|-------|
| STM32H753 | [Renode](https://renode.io) | `renode/body_ecu.resc` | Uses nucleo_h753zi.repl (single-core sibling) |

The Renode platform uses the STM32H753 (single-core sibling of the dual-core H755)
because Renode has an existing platform description for it. OpenBSW runs on the M7
core only, so single-core emulation is sufficient.

## HPC Migration Path

For deployment on a Linux HPC (MPU), the POSIX build is the starting point:

- `ConsoleGpioAdapter` -> Linux sysfs/gpiod for real GPIO
- `SocketCanAdapter` -> Already production-ready SocketCAN
- `StdinButtonAdapter` -> Linux input event subsystem
- Domain logic (`libs/body/`) and platform modules (`libs/platform/`) compile unchanged
