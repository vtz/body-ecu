# Body ECU Demo — Executive Brief

## Overview

This demo showcases a **distributed Body ECU** spanning two processors that communicate over Ethernet, with full cloud connectivity. It demonstrates how safety-critical vehicle functions running on a real-time microcontroller can be monitored and controlled remotely through a cloud-connected web application — all built on open-source technologies and aligned with automotive industry standards.

## What It Does

A user opens a **web dashboard** on their laptop and can:

- **Lock and unlock the vehicle door** — the command travels from the browser through NATS messaging, into the Linux MPU, across SOME/IP over Ethernet to the Zephyr MCU, which actuates the lock GPIO and confirms the state change back through the entire chain.
- **Toggle headlights, turn signals, and brake lights** — each light can be switched on/off from the dashboard, with real-time status feedback.
- **Monitor vehicle mode** — see live ignition state transitions (OFF → ACC → RUN → CRANK) as a physical button on the NUCLEO board is pressed.
- **View vehicle speed** — the MCU reads a potentiometer via ADC and broadcasts speed over SOME/IP; the dashboard shows it in real time.
- **See the VIN** — fetched dynamically from the MCU firmware at startup via SOME/IP.
- **Watch a live event log and NATS message trace** — all state changes and protocol messages are visible in the dashboard.

## Architecture

```
┌─────────────┐       ┌──────────────────────────────────────┐       ┌──────────────────────┐
│  Web App    │       │         MPU (AutoSD / Linux)         │       │  MCU (Zephyr RTOS)   │
│  (Flask)    │       │                                      │       │  NUCLEO-H755ZI-Q     │
│             │ NATS  │  Cloud         SomeIp-Kuksa   SOME/IP│  UDP  │                      │
│  Dashboard ◄──────► │  Gateway ◄───► Bridge ◄──────────────┼──────►│  Lighting            │
│  REST API   │       │  Client        Kuksa                 │       │  Door Lock           │
│             │       │                Databroker (gRPC)     │       │  Vehicle Mode        │
└─────────────┘       └──────────────────────────────────────┘       │  Speed Simulator     │
     Mac                      QEMU VM (aarch64)                      │  VIN Provider        │
                                                                     │  UDS Diagnostics     │
                                                                     └──────────────────────┘
                                                                           NUCLEO Board
```

### Protocol Stack (end-to-end for a door lock command)

```
Browser → HTTP POST → Flask → NATS publish → SSH tunnel → NATS server (VM)
→ CloudGatewayClient → Kuksa gRPC publish → SomeIpKuksaBridge
→ SOME/IP UDP request → MCU DoorLockController → GPIO actuation
→ SOME/IP event → Bridge → Kuksa → NATS → Flask → Browser (status update)
```

### Software Architecture

The project follows **hexagonal architecture** (ports & adapters):

| Layer | Location | Description |
|-------|----------|-------------|
| Port Interfaces | `libs/platform/ports/` | Abstract C++ interfaces — zero dependencies |
| Domain Logic | `libs/body/` | Portable business logic (lighting, door lock, vehicle mode, speed) |
| Platform Services | `libs/platform/` | CAN gateway, UDS diagnostics, cloud gateway |
| Adapters | `libs/adapters/` | Platform-specific implementations |

Domain logic depends **only** on port interfaces and can run on any target. Swapping adapters changes the platform without touching business logic.

| Platform | Signal Bus | Cloud Transport | SOME/IP Role |
|----------|-----------|-----------------|--------------|
| Zephyr MCU | `LocalSignalBus` | N/A | Server |
| AutoSD MPU | `KuksaSignalBusAdapter` (gRPC) | `NatsCloudTransportAdapter` | Client |
| POSIX (dev) | `InProcessSignalBus` | `StubCloudTransport` | Either |

### Key Technologies

| Technology | Role |
|------------|------|
| **Zephyr RTOS** | Real-time OS on STM32H755 MCU |
| **Red Hat AutoSD** | Automotive Linux distribution on MPU |
| **SOME/IP** | Inter-processor communication (UDP) |
| **Eclipse Kuksa** | VSS-compliant vehicle signal databroker (gRPC) |
| **NATS** | Cloud messaging between MPU and companion app |
| **Flask** | Web dashboard and REST API |
| **OpenBSW** | Lifecycle management and async framework |

### SOME/IP Service Catalog

| Service | ID | Methods | Events |
|---------|----|---------|--------|
| Lighting | 0x1000 | SetLightState, GetLightStatus | LightStatusChanged |
| Door Lock | 0x1001 | Lock, Unlock, GetStatus | LockStateChanged |
| Vehicle Mode | 0x1002 | GetMode, SetMode | ModeNotifier |
| Speed Sensor | 0x1003 | GetSpeed, SetSpeed | SpeedChanged |
| Vehicle Info | 0x1004 | GetVIN | — |

### VSS Signal Map

| VSS Path | Type | Direction |
|----------|------|-----------|
| `Vehicle.Mode` | int32 | MCU → Cloud |
| `Vehicle.Speed` | float | MCU → Cloud |
| `Vehicle.Lights.Status` | int32 (bitmask) | MCU → Cloud |
| `Vehicle.Cabin.Door.Row1.DriverSide.IsLocked` | boolean | MCU → Cloud |
| `Vehicle.Command.Door.Lock` | boolean | Cloud → MCU |
| `Vehicle.Command.Lights.Set` | int32 (packed) | Cloud → MCU |

## What This Demonstrates

1. **Mixed-criticality deployment** — safety-critical functions on a certified RTOS, cloud connectivity on Linux, clean separation via SOME/IP.
2. **Hexagonal architecture in practice** — the same door lock logic runs unchanged on Zephyr, AutoSD, and POSIX by swapping adapters.
3. **VSS + Kuksa integration** — vehicle signals follow the COVESA Vehicle Signal Specification standard.
4. **End-to-end cloud control** — a web browser can actuate real hardware through multiple protocol translations with sub-second latency.
5. **RPM packaging for AutoSD** — the MPU software is packaged as an RPM with bundled dependencies, ready for automotive Linux deployment.
6. **Code generation from YAML** — SOME/IP service IDs, method IDs, and bridge mappings are generated from a single `services.yaml` source of truth.
