# Body ECU Spike

A modular Body ECU application spanning two processors -- an **MCU**
(NUCLEO-H755ZI-Q / Zephyr) for real-time body control and an **MPU**
(Red Hat AutoSD / Linux) for vehicle signal brokering and cloud connectivity.

Built on [OpenBSW-Zephyr](https://github.com/eclipse-openbsw/openbsw-zephyr)
and [OpenSOME/IP](https://github.com/vtz/opensomeip).

## Deployment Topology

```
  Cloud                         MPU (AutoSD / Linux)          MCU (Nucleo / Zephyr)
 ┌──────────────┐             ┌──────────────────┐          ┌──────────────────────┐
 │ NATS broker  │◄──NATS─────►│CLOUD_GATEWAY     │          │ LOCKING_SERVICE      │
 │              │             │  _CLIENT          │          │ LIGHTING             │
 └──────────────┘             │                  │          │ VEHICLE_MODE         │
                              │ DATA_BROKER      │◄─SOME/IP►│ SPEED_SIMULATOR      │
                              │ (Kuksa gRPC)     │  bridge  │ CAN_GATEWAY          │
                              │                  │          │ DIAGNOSTICS          │
                              │ SomeIpKuksa      │          │                      │
                              │  Bridge          │          │ SOME/IP server       │
                              └──────────────────┘          └──────────────────────┘
```

The MCU runs safety-critical body services and exposes them via SOME/IP.
The MPU bridges those services to the [Eclipse Kuksa](https://github.com/eclipse-kuksa/kuksa-databroker)
Databroker (VSS-compliant signal bus) and to the cloud via [NATS](https://nats.io).

## Features

- **Exterior Lighting** -- SOME/IP service controlling on-board LEDs (headlight, turn, brake)
- **Door Lock** -- state machine with SOME/IP control, button toggle, and cloud lock/unlock via signal bus
- **Vehicle Mode** -- getter/setter/notifier field (Off, Accessory, Run, Crank)
- **Speed Simulator** -- potentiometer-driven accelerator simulation, broadcasts speed every 100ms via SOME/IP
- **SOME/IP-to-CAN Gateway** -- bidirectional message translation driven by YAML config
- **UDS Diagnostics** -- ReadDataByID, IOControl, ReadDTC over DoIP (Ethernet) and DoCAN (CAN-FD)
- **Cloud Gateway Client** -- bridges NATS cloud commands to the VSS signal bus (MPU only)
- **Kuksa Integration** -- VSS signal pub/sub via gRPC to Kuksa Databroker (MPU only)

## Architecture

The project follows a **hexagonal (ports & adapters)** architecture:

| Layer | Path | Purpose |
|-------|------|---------|
| Port interfaces | `libs/platform/ports/` | Abstract C++ interfaces (no deps) |
| Domain logic | `libs/body/` | Portable business logic (lighting, door-lock, vehicle-mode) |
| ECU-generic | `libs/platform/` | Reusable modules (config-loader, can-gateway, diagnostics, cloud-gateway) |
| Adapters | `libs/adapters/` | Platform-specific (Zephyr, Linux, AutoSD) |

Domain modules depend **only** on port interfaces and can be tested on any host or
migrated between MCU and MPU by swapping adapters.

| Platform | Signal Bus | Cloud Transport | Role |
|----------|-----------|-----------------|------|
| Zephyr (MCU) | `LocalSignalBus` | N/A (via MPU) | SOME/IP server |
| AutoSD (MPU) | `KuksaSignalBusAdapter` (gRPC) | `NatsCloudTransportAdapter` | SOME/IP client |
| POSIX (dev) | `InProcessSignalBus` | `StubCloudTransport` or real NATS | Dev / test |

## Quick Start

### Prerequisites

- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) (for MCU builds)
- CMake >= 3.20, Ninja
- Python 3.10+
- `west` (`pip install west`)
- Docker (for cloud connectivity testing)

### Set up the workspace

```bash
west init -l .
west update
```

### Two-Process POSIX Development (MCU + MPU)

The recommended development setup runs both processors as separate POSIX
processes communicating over SOME/IP via localhost:

```bash
# Terminal 1 -- MCU side
cmake -B build/posix-mcu -S platforms/posix-mcu && cmake --build build/posix-mcu
./build/posix-mcu/body_ecu_posix_mcu

# Terminal 2 -- MPU side
cmake -B build/posix-mpu -S platforms/posix-mpu && cmake --build build/posix-mpu
./build/posix-mpu/body_ecu_posix_mpu 127.0.0.1
```

### Cloud Connectivity Test Environment

To test the full MPU-to-cloud path with real NATS and Kuksa:

```bash
# Start NATS + Kuksa Databroker containers
docker compose -f docker-compose.test.yml up -d

# Build MPU with real adapters
cmake -B build/posix-mpu -S platforms/posix-mpu -DBODY_ECU_REAL_ADAPTERS=ON
cmake --build build/posix-mpu

# Start the companion app simulator
cd services/companion-app && pip install -r requirements.txt
python app.py &

# Run integration tests
pytest tests/integration/ -v
```

### Build for POSIX (single-process, no Zephyr required)

```bash
cmake -B build/posix -S platforms/posix
cmake --build build/posix
./build/posix/body_ecu_posix [vcan0]
```

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
