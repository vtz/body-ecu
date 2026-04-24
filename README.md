# Body ECU Spike

A modular Body ECU application built on the **NUCLEO-H755ZI-Q** using
[OpenBSW-Zephyr](https://github.com/eclipse-openbsw/openbsw-zephyr) and
[OpenSOME/IP](https://github.com/vtz/opensomeip).

## Features

- **Exterior Lighting** -- SOME/IP service controlling on-board LEDs (headlight, turn, brake)
- **Door Lock** -- state machine with SOME/IP control and button toggle
- **Vehicle Mode** -- getter/setter/notifier field (Off, Accessory, Run, Crank)
- **Speed Simulator** -- potentiometer-driven accelerator simulation, broadcasts speed every 100ms via SOME/IP
- **SOME/IP-to-CAN Gateway** -- bidirectional message translation driven by YAML config
- **UDS Diagnostics** -- ReadDataByID, IOControl, ReadDTC over DoIP (Ethernet) and DoCAN (CAN-FD)

## Architecture

The project follows a **hexagonal (ports & adapters)** architecture:

| Layer | Path | Purpose |
|-------|------|---------|
| Port interfaces | `libs/platform/ports/` | Abstract C++ interfaces (no deps) |
| Domain logic | `libs/body/` | Portable business logic (lighting, door-lock, vehicle-mode) |
| ECU-generic | `libs/platform/` | Reusable modules (config-loader, can-gateway, diagnostics) |
| Adapters | `libs/adapters/` | Platform-specific (Zephyr, Linux, OpenBSW lifecycle) |

Domain modules depend **only** on port interfaces and can be tested on any host or migrated
to an HPC (MPU) by swapping adapters.

## Quick Start

### Prerequisites

- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
- CMake >= 3.20, Ninja
- Python 3.10+
- `west` (`pip install west`)

### Set up the workspace

```bash
west init -l .
west update
```

### Build for POSIX (Linux/macOS -- no Zephyr required)

```bash
cmake -B build/posix -S platforms/posix
cmake --build build/posix
./build/posix/body_ecu_posix [vcan0]
```

Uses real sockets for SOME/IP, SocketCAN for CAN, console for GPIO, and stdin for button input.

### Build for native_sim (Zephyr simulation)

```bash
west build -b native_sim app
west build -t run
```

### Build for NUCLEO-H755ZI-Q (hardware)

```bash
west build -b nucleo_h755zi_q/stm32h755xx/m7 app
west flash
```

### Run host-based unit tests

```bash
cmake -B build/tests -S tests/unit -DBUILD_TESTS=ON
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

## Documentation

- [Build instructions](docs/BUILD.md)
- [Architecture overview](docs/architecture/overview.md)
- [Design decisions](docs/decisions/)
- [Supported platforms](docs/SUPPORTED_PLATFORMS.md)

## License

[Apache-2.0](LICENSE)
