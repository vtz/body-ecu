# Body ECU -- Product Requirements Document

## Abstract

The Body ECU provides safety-relevant body functions (door locking, exterior
lighting, vehicle mode management) and bridges them to cloud services via a
vehicle signal bus and a cloud gateway. The architecture is designed for a
mixed-criticality deployment where an **MCU** (NUCLEO-H755ZI-Q running Zephyr)
handles real-time, safety-critical body control while an **MPU** (running Red
Hat Automotive Stream Distribution / AutoSD) hosts the vehicle signal broker
and cloud connectivity.

All software components follow a hexagonal (ports & adapters) architecture so
that any component can be deployed on either the MPU or MCU by swapping
adapters and updating a deployment descriptor. Domain logic has zero
dependencies on platform APIs.

This document is derived from the
[parking-fee-service PRD](https://github.com/rhadp/parking-fee-service/blob/develop/.specs/prd.md),
scoped to the body-ECU-side components that the parking-fee demo requires.

## User Journey (Body ECU Perspective)

1. **Vehicle start-up:** MCU boots Zephyr, initializes body services (lighting,
   door lock, vehicle mode, CAN gateway, diagnostics). MPU boots AutoSD,
   starts DATA_BROKER (Kuksa) and CLOUD_GATEWAY_CLIENT.
2. **Remote lock command:** A COMPANION_APP sends a lock command via the cloud.
   CLOUD_GATEWAY receives it and forwards over NATS to
   CLOUD_GATEWAY_CLIENT on the MPU. CLOUD_GATEWAY_CLIENT validates the command
   and publishes `Vehicle.Command.Door.Lock` to the DATA_BROKER. The signal
   propagates to the MCU's LOCKING_SERVICE via the CAN signal bridge.
3. **Safety validation:** LOCKING_SERVICE checks safety constraints (vehicle
   speed, door ajar status) before actuating the lock.
4. **Lock actuation:** LOCKING_SERVICE drives the lock GPIO, updates state, and
   publishes `Vehicle.Cabin.Door.Row1.DriverSide.IsLocked` to the signal bus.
5. **State propagation:** The lock state reaches the DATA_BROKER on the MPU,
   which CLOUD_GATEWAY_CLIENT forwards to the cloud. The PARKING_OPERATOR_ADAPTOR
   (outside this repo's scope) subscribes to this event and starts a parking
   session autonomously.
6. **Local interaction:** A physical button press on the Nucleo board toggles the
   lock via the existing `IButtonInput` path, with the same signal bus
   publication.

## Problem Statement

The existing Body ECU spike provides body functions (lighting, door lock,
vehicle mode) over SOME/IP and UDS diagnostics. It lacks:

- **Vehicle signal bus integration** -- no VSS-compliant pub/sub to share state
  across partitions and domains.
- **Cloud connectivity** -- no path for remote lock/unlock commands from a
  companion app or cloud service.
- **MPU deployment target** -- the POSIX build is a development aid, not a
  production MPU image.
- **Deployment flexibility** -- components are hard-wired to a single target;
  there is no mechanism to redistribute them between MPU and MCU.

## Architecture Overview

### Deployment Topology

```
  Cloud                         MPU (AutoSD)                MCU (Nucleo / Zephyr)
 ┌──────────────┐             ┌──────────────────┐        ┌──────────────────────┐
 │ CLOUD_GATEWAY│◄──NATS─────►│CLOUD_GATEWAY     │        │ LOCKING_SERVICE      │
 │              │             │  _CLIENT          │        │ LIGHTING             │
 └──────────────┘             │                  │        │ VEHICLE_MODE         │
                              │ DATA_BROKER      │◄─CAN──►│ CAN_GATEWAY          │
                              │ (Kuksa)          │  bridge│ DIAGNOSTICS          │
                              │                  │        │                      │
                              │ Kuksa gRPC       │        │ Local Signal Bus     │
                              │  Adapter         │        │ CAN Signal Bridge    │
                              └──────────────────┘        └──────────────────────┘
```

### Component Placement (Default)

| Component              | Target | Partition / ASIL   | Protocol Exposure             |
|------------------------|--------|--------------------|-------------------------------|
| LOCKING_SERVICE        | MCU    | Safety (ASIL-B)    | SOME/IP, Signal Bus, GPIO     |
| LIGHTING               | MCU    | Safety             | SOME/IP, Signal Bus, GPIO     |
| VEHICLE_MODE           | MCU    | Safety             | SOME/IP, Signal Bus           |
| CAN_GATEWAY            | MCU    | --                 | CAN-FD, SOME/IP               |
| DIAGNOSTICS            | MCU    | --                 | DoIP, DoCAN (UDS)             |
| DATA_BROKER            | MPU    | QM                 | gRPC (Kuksa val.proto)        |
| CLOUD_GATEWAY_CLIENT   | MPU    | QM                 | NATS, gRPC (Kuksa)            |

Any component can be moved to the other target by changing the deployment
descriptor and using the matching adapter set.

### Hexagonal Architecture

```
 Adapters ──► Port Interfaces ◄── Domain Logic
                    ▲
                    │
              ECU-Generic
```

| Layer              | Path                    | Purpose                                              |
|--------------------|-------------------------|------------------------------------------------------|
| Port Interfaces    | `libs/platform/ports/`  | Abstract C++ interfaces (zero dependencies)          |
| Domain Logic       | `libs/body/`            | Portable body functions (lighting, door-lock, etc.)  |
| ECU-Generic        | `libs/platform/`        | Reusable modules (config-loader, CAN gw, diag, cloud gw) |
| Adapters           | `libs/adapters/`        | Platform-specific (Zephyr, Linux, AutoSD, OpenBSW)   |
| Applications       | `app/`, `platforms/`    | Entry points and wiring per target                   |

Domain modules depend **only** on port interfaces. They never include Zephyr,
OpenBSW, Kuksa, or NATS headers.

### Mixed-Criticality Communication

- Safety-critical services on the MCU publish state to a **Local Signal Bus**
  (lightweight in-process store).
- A **CAN Signal Bridge** maps VSS signal paths to CAN frame IDs and exchanges
  them with the MPU.
- On the MPU, the **Kuksa gRPC Adapter** translates CAN-bridged signals into
  Kuksa Databroker API calls.
- QM services on the MPU (CLOUD_GATEWAY_CLIENT) access the DATA_BROKER via
  gRPC, reading safety-relevant state in read-only mode.

## Components

### LOCKING_SERVICE

Evolution of the existing `DoorLockController`. Runs on the MCU in the safety
partition.

**Responsibilities:**

- State machine: Locked / Unlocked / Error.
- Subscribes to `Vehicle.Command.Door.Lock` via the signal bus for remote
  lock/unlock commands originating from CLOUD_GATEWAY_CLIENT.
- Validates safety constraints before executing lock/unlock:
  - `Vehicle.Speed` must be zero (stationary vehicle).
  - `Vehicle.Cabin.Door.Row1.DriverSide.IsOpen` must be false (door closed).
- Actuates lock GPIO on state change.
- Publishes `Vehicle.Cabin.Door.Row1.DriverSide.IsLocked` to the signal bus
  on every state transition.
- Publishes `Vehicle.Command.Door.Response` with result code.
- Retains existing SOME/IP interface (Lock, Unlock, GetStatus methods;
  LockStateChanged event) for backward compatibility.
- Retains existing button toggle via `IButtonInput`.
- Retains `IDiagDataProvider` for UDS ReadDataByID / IOControl.

**Port dependencies:** `IGpioPort`, `IButtonInput`, `ISomeIpService`,
`ISignalBus` (optional, nullable for backward-compatible builds).

### DATA_BROKER

Eclipse Kuksa Databroker, deployed as a pre-built binary on the MPU. Not
reimplemented. Provides:

- VSS-compliant gRPC pub/sub interface for vehicle signals.
- Signal state management and read/write access control.
- Cross-partition consumers access via network TCP (gRPC over HTTP/2).

The Body ECU integrates with it through the `ISignalBus` port interface,
implemented by `KuksaSignalBusAdapter` on the MPU and bridged to the MCU
over CAN.

### CLOUD_GATEWAY_CLIENT

New platform module. Runs on the MPU. Bridges vehicle signals and cloud
commands.

**Responsibilities:**

- Connects to the cloud NATS server via `ICloudTransport`.
- Subscribes to NATS subject `vehicles.{vin}.command.door.lock`.
- Validates incoming commands (bearer token, schema).
- Publishes validated commands to `ISignalBus` at
  `Vehicle.Command.Door.Lock`.
- Subscribes to signal bus state changes
  (`Vehicle.Cabin.Door.Row1.DriverSide.IsLocked`) and forwards them to cloud
  via NATS at `vehicles.{vin}.state.door.locked`.
- Publishes command responses back to NATS at
  `vehicles.{vin}.command.door.response`.

**Port dependencies:** `ISignalBus`, `ICloudTransport`.

### LIGHTING, VEHICLE_MODE, CAN_GATEWAY, DIAGNOSTICS

Existing components. Enhanced with optional `ISignalBus` integration to
publish state to the vehicle signal bus. No changes to their core logic.

## Port Interfaces

### Existing (unchanged)

| Interface          | Purpose                                    |
|--------------------|--------------------------------------------|
| `IGpioPort`        | Digital output read/write                  |
| `ICanBus`          | CAN frame send/receive with RX callback    |
| `ISomeIpService`   | SOME/IP method registration, events        |
| `IButtonInput`     | Button press callback registration         |
| `IDiagDataProvider`| UDS diagnostic data read and I/O control   |
| `IModeObserver`    | Vehicle mode change notifications          |
| `ITimerService`    | Periodic and one-shot timer scheduling     |

### New

| Interface          | Purpose                                    |
|--------------------|--------------------------------------------|
| `ISignalBus`       | VSS-compliant signal pub/sub abstraction   |
| `ICloudTransport`  | Cloud messaging abstraction (NATS-like)    |

#### ISignalBus

```cpp
class ISignalBus {
public:
    virtual ~ISignalBus() = default;
    virtual bool publish(const std::string& path,
                         const SignalValue& value) = 0;
    virtual void subscribe(const std::string& path,
                           SignalCallback callback) = 0;
    virtual std::optional<SignalValue> get(const std::string& path) const = 0;
};
```

`SignalValue` is a variant-like type holding `bool`, `int32_t`, `float`,
`std::string`, or `std::vector<uint8_t>`.

Signal paths follow the COVESA VSS convention:
`Vehicle.Cabin.Door.Row1.DriverSide.IsLocked`.

#### ICloudTransport

```cpp
class ICloudTransport {
public:
    virtual ~ICloudTransport() = default;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool publish(const std::string& subject,
                         const std::vector<uint8_t>& data) = 0;
    virtual void subscribe(const std::string& subject,
                           CloudMessageCallback callback) = 0;
};
```

Subjects follow the pattern `vehicles.{vin}.{action}`.

## Adapters

### AutoSD / MPU -- `libs/adapters/autosd/`

| Adapter                    | Port Interface   | Backend                          |
|----------------------------|------------------|----------------------------------|
| `KuksaSignalBusAdapter`    | `ISignalBus`     | gRPC client to Kuksa Databroker  |
| `NatsCloudTransportAdapter`| `ICloudTransport`| nats.c client library            |
| `SystemdLifecycleAdapter`  | (lifecycle)      | systemd notify protocol          |

### Zephyr / MCU -- `libs/adapters/zephyr/` (extended)

| Adapter            | Port Interface   | Backend                                      |
|--------------------|------------------|----------------------------------------------|
| `GpioAdapter`      | `IGpioPort`      | Zephyr GPIO driver (existing)                |
| `CanAdapter`       | `ICanBus`        | Zephyr CAN driver (existing)                 |
| `ButtonAdapter`    | `IButtonInput`   | Zephyr GPIO interrupt (existing)             |
| `LocalSignalBus`   | `ISignalBus`     | In-process signal store + observer pattern   |
| `CanSignalBridge`  | --               | Bridges LocalSignalBus to CAN frames via ICanBus |

### Linux / POSIX -- `libs/adapters/linux/` (extended)

| Adapter              | Port Interface   | Backend                           |
|----------------------|------------------|-----------------------------------|
| `ConsoleGpioAdapter` | `IGpioPort`      | Console stdout (existing)         |
| `SocketCanAdapter`   | `ICanBus`        | SocketCAN (existing)              |
| `StdinButtonAdapter` | `IButtonInput`   | stdin Enter key (existing)        |
| `InProcessSignalBus` | `ISignalBus`     | In-process store (same as MCU)    |
| `StubCloudTransport` | `ICloudTransport`| Logs to stdout (dev/test only)    |

## VSS Signal Schema

Custom VSS overlay signals used by body services:

| Signal Path                                      | Type     | Datatype | Producer             | Consumer(s)                    |
|--------------------------------------------------|----------|----------|----------------------|--------------------------------|
| `Vehicle.Cabin.Door.Row1.DriverSide.IsLocked`    | actuator | boolean  | LOCKING_SERVICE      | CLOUD_GATEWAY_CLIENT, PARKING_OPERATOR_ADAPTOR |
| `Vehicle.Command.Door.Lock`                      | actuator | boolean  | CLOUD_GATEWAY_CLIENT | LOCKING_SERVICE                |
| `Vehicle.Command.Door.Response`                  | sensor   | uint8    | LOCKING_SERVICE      | CLOUD_GATEWAY_CLIENT           |
| `Vehicle.Cabin.Door.Row1.DriverSide.IsOpen`      | sensor   | boolean  | (hardware sensor)    | LOCKING_SERVICE                |
| `Vehicle.Speed`                                  | sensor   | float    | (powertrain ECU)     | LOCKING_SERVICE                |
| `Vehicle.Parking.SessionActive`                  | sensor   | boolean  | PARKING_OPERATOR_ADAPTOR | (outside scope)            |

The overlay is defined in `config/vss_overlay.yaml` and loaded by the
DATA_BROKER (Kuksa) on the MPU.

## Configuration

### Service Configuration (`config/services.yaml`)

Existing SOME/IP service definitions (lighting, door_lock, vehicle_mode) are
retained. New section added:

```yaml
cloud_gateway:
  vin: "WVWZZZ3CZWE000001"
  nats_url: "nats://localhost:4222"
  subjects:
    command_door_lock: "vehicles.{vin}.command.door.lock"
    command_door_response: "vehicles.{vin}.command.door.response"
    state_door_locked: "vehicles.{vin}.state.door.locked"
```

### Deployment Descriptor (`config/deployment.yaml`)

Declares which components run on which target and which adapter backends to
use:

```yaml
targets:
  mcu:
    platform: zephyr
    board: nucleo_h755zi_q/stm32h755xx/m7
    components:
      - locking_service
      - lighting
      - vehicle_mode
      - can_gateway
      - diagnostics
    signal_bus: local
    cloud_transport: none

  mpu:
    platform: autosd
    components:
      - cloud_gateway_client
      - data_broker_client
    signal_bus: kuksa
    cloud_transport: nats
```

### CAN Signal Bridge Mapping (`config/signal_bridge.yaml`)

Maps VSS signal paths to CAN frame IDs for cross-processor exchange:

```yaml
mappings:
  - signal: "Vehicle.Cabin.Door.Row1.DriverSide.IsLocked"
    can_id: 0x400
    dlc: 1
    byte_offset: 0

  - signal: "Vehicle.Command.Door.Lock"
    can_id: 0x401
    dlc: 1
    byte_offset: 0

  - signal: "Vehicle.Command.Door.Response"
    can_id: 0x402
    dlc: 1
    byte_offset: 0
```

## Build Targets

| Target            | Build Command                                           | Signal Bus         | Cloud Transport    | SOME/IP Role |
|-------------------|---------------------------------------------------------|--------------------|--------------------|--------------|
| MCU (Zephyr)      | `west build -b nucleo_h755zi_q/stm32h755xx/m7 app`     | LocalSignalBus     | none (via MPU)     | server       |
| MPU (AutoSD)      | `cmake -B build/autosd -S platforms/autosd`             | KuksaSignalBus     | NATS               | client       |
| POSIX MCU (dev)   | `cmake -B build/posix-mcu -S platforms/posix-mcu`      | InProcessSignalBus | none               | server       |
| POSIX MPU (dev)   | `cmake -B build/posix-mpu -S platforms/posix-mpu`      | InProcessSignalBus | StubCloudTransport | client       |
| native_sim        | `west build -b native_sim app`                          | LocalSignalBus     | none               | server       |
| Unit tests        | `cmake -B build/tests -S tests/unit -DBUILD_TESTS=ON`  | Mocks              | Mocks              | --           |

## Development Environment

### Two-Process POSIX Setup

For day-to-day development without hardware or VMs, the body ECU runs as two
separate POSIX processes on the host that communicate over SOME/IP via
localhost:

```
 ┌─────────────────────────┐         SOME/IP (UDP)        ┌─────────────────────────┐
 │  body_ecu_posix_mcu     │◄────── localhost:30490 ──────►│  body_ecu_posix_mpu     │
 │                         │                              │                         │
 │  LOCKING_SERVICE        │                              │  SOMEIP_KUKSA_BRIDGE    │
 │  LIGHTING               │                              │  CLOUD_GATEWAY_CLIENT   │
 │  VEHICLE_MODE           │                              │  InProcessSignalBus     │
 │  CAN_GATEWAY            │                              │  StubCloudTransport     │
 │  DIAGNOSTICS            │                              │                         │
 │  SomeIpSystem (server)  │                              │  SomeIpSystem (client)  │
 │  InProcessSignalBus     │                              │                         │
 │  ConsoleGpioAdapter     │                              │                         │
 │  StdinButtonAdapter     │                              │                         │
 └─────────────────────────┘                              └─────────────────────────┘
```

**POSIX MCU process** (`platforms/posix-mcu/`): Runs all body services with
SOME/IP server on port 30490. Reuses the existing Linux adapters
(ConsoleGpio, SocketCAN, StdinButton). This is an evolution of the current
`platforms/posix/` build.

**POSIX MPU process** (`platforms/posix-mpu/`): Runs `SomeIpKuksaBridge` as a
SOME/IP client connecting to the MCU process, plus `CloudGatewayClient` with
`InProcessSignalBus` and `StubCloudTransport`. No GPIO, CAN, or button
adapters.

Usage:

```bash
# Terminal 1 -- MCU side
cmake -B build/posix-mcu -S platforms/posix-mcu
cmake --build build/posix-mcu
./build/posix-mcu/body_ecu_posix_mcu

# Terminal 2 -- MPU side
cmake -B build/posix-mpu -S platforms/posix-mpu
cmake --build build/posix-mpu
./build/posix-mpu/body_ecu_posix_mpu
```

The existing single-process `platforms/posix/` build is retained for
quick local testing where the MPU bridge is not needed.

### QEMU + Renode Virtual Integration

For integration testing closer to the target hardware, the MCU firmware
runs in Renode and the MPU software runs in a QEMU VM with AutoSD. Both
connect to a shared virtual Ethernet so that SOME/IP traffic flows between
them as it would on real hardware.

```
 Host machine
 ┌──────────────────────────────────────────────────────────────────────┐
 │                                                                      │
 │  ┌──────────────────┐       TAP bridge        ┌───────────────────┐ │
 │  │  Renode           │       (br-ecu)          │  QEMU             │ │
 │  │  STM32H753        │                         │  AutoSD (aarch64) │ │
 │  │                   │    ┌──────────────┐     │                   │ │
 │  │  body_ecu.elf     │    │              │     │  Kuksa Databroker │ │
 │  │  Ethernet MAC ────┼────┤  br-ecu      ├────┼── eth0            │ │
 │  │  192.168.100.10   │    │  192.168.100.1│    │  192.168.100.20   │ │
 │  │                   │    └──────────────┘     │                   │ │
 │  │  SOME/IP server   │                         │  SOMEIP_KUKSA     │ │
 │  │  :30490           │◄──── SOME/IP (UDP) ────►│   _BRIDGE         │ │
 │  │                   │                         │  CLOUD_GATEWAY    │ │
 │  └──────────────────┘                         │   _CLIENT         │ │
 │                                                └───────────────────┘ │
 │                                                                      │
 │  Host can also attach to br-ecu for debugging (candump, wireshark)   │
 └──────────────────────────────────────────────────────────────────────┘
```

#### Network Setup

A helper script (`scripts/vnet_setup.sh`) creates the shared virtual
network:

1. Creates a Linux bridge `br-ecu` with IP `192.168.100.1/24`.
2. Creates TAP interfaces `tap-renode` and `tap-qemu`.
3. Attaches both TAPs to `br-ecu`.

#### Renode Configuration

The Renode script (`renode/body_ecu_vnet.resc`) extends `body_ecu.resc` to
connect the emulated Ethernet MAC to `tap-renode`:

```
emulation CreateTap "tap-renode" "tap-renode"
connector Connect sysbus.ethernet tap-renode
```

The firmware uses a static IP `192.168.100.10` configured via Zephyr
prj.conf overlay or board-specific config.

#### QEMU Launch

A helper script (`scripts/run_qemu_autosd.sh`) launches the AutoSD VM:

```bash
qemu-system-aarch64 \
    -machine virt -cpu cortex-a57 -m 2G \
    -drive file=autosd.qcow2,format=qcow2 \
    -netdev tap,id=net0,ifname=tap-qemu,script=no,downscript=no \
    -device virtio-net-pci,netdev=net0 \
    ...
```

The AutoSD VM is configured with static IP `192.168.100.20` on its
`eth0` interface. The `SomeIpKuksaBridge` connects to the MCU's SOME/IP
server at `192.168.100.10:30490`.

#### Integration Test Flow

1. `scripts/vnet_setup.sh` creates the bridge and TAPs.
2. Renode launches with `body_ecu_vnet.resc` (MCU firmware).
3. QEMU launches with AutoSD image (MPU services).
4. Test harness (Python/pytest) runs on the host, connected to `br-ecu`:
   - Sends NATS messages to simulate cloud commands (via NATS container
     on the host, also attached to `br-ecu` or on `localhost`).
   - Verifies SOME/IP events via a Python SOME/IP client.
   - Verifies Kuksa signals via Kuksa gRPC client.
   - Checks round-trip: cloud command -> NATS -> MPU -> SOME/IP -> MCU
     lock -> SOME/IP event -> MPU -> Kuksa signal -> NATS response.

#### CI Considerations

The QEMU + Renode integration test is heavyweight and runs as a separate
CI job gated on the `integration` label or nightly schedule. The two-process
POSIX setup is used for faster CI feedback.

## Dependencies

### West Manifest (existing)

- Zephyr RTOS (pinned revision)
- Eclipse OpenBSW
- openbsw-zephyr
- OpenSOME/IP v0.0.5

### West Manifest (new)

- Eclipse Kuksa Databroker proto definitions (for gRPC stub generation)
- nats.c (C client library for NATS)

### FetchContent (unit tests)

- GoogleTest v1.14.0 (existing)
- yaml-cpp 0.8.0 (existing)

## Implementation Phases

### Phase 1: Port Interfaces

Add `ISignalBus` and `ICloudTransport` to `libs/platform/ports/`. Add
corresponding GoogleMock implementations to `libs/platform/ports/mock/`.

### Phase 2: LOCKING_SERVICE

Enhance `DoorLockController` with optional `ISignalBus` dependency, safety
constraint validation, and signal bus pub/sub. Preserve all existing SOME/IP,
button, and diagnostics interfaces.

### Phase 3: Cloud Gateway Client

Create `CloudGatewayClient` in `libs/platform/cloud-gateway/`. Wire
`ICloudTransport` to `ISignalBus` with command validation and bidirectional
state forwarding.

### Phase 4: VSS Signal Schema

Create `config/vss_overlay.yaml` and `config/signal_bridge.yaml`. Add
`cloud_gateway` section to `config/services.yaml`.

### Phase 5: Adapters

Implement adapter sets for all platforms:
- AutoSD/MPU: `KuksaSignalBusAdapter`, `NatsCloudTransportAdapter`,
  `SomeIpKuksaBridge`
- Zephyr/MCU: `LocalSignalBus` (internal only; cross-processor uses existing
  SOME/IP)
- POSIX/dev: `InProcessSignalBus`, `StubCloudTransport`

### Phase 6: Two-Process POSIX Development Builds

Split the existing `platforms/posix/` into two builds:
- `platforms/posix-mcu/`: body services with SOME/IP server (evolution of
  existing `platforms/posix/`).
- `platforms/posix-mpu/`: `SomeIpKuksaBridge` + `CloudGatewayClient` as
  SOME/IP client.
Retain existing `platforms/posix/` as a single-process convenience build.

### Phase 7: Deployment Configuration and Build System

Create `config/deployment.yaml`. Add `platforms/autosd/` build target. Update
`west.yml` with new dependencies. Wire new adapters into application entry
points.

### Phase 8: QEMU + Renode Virtual Integration

Create virtual networking scripts (`scripts/vnet_setup.sh`), extended
Renode script (`renode/body_ecu_vnet.resc`), and QEMU launch script
(`scripts/run_qemu_autosd.sh`). Validate SOME/IP communication between
Renode (MCU) and QEMU/AutoSD (MPU) over TAP bridge.

### Phase 9: Testing

Unit tests for all new components (GoogleTest). Two-process POSIX
integration tests. QEMU + Renode end-to-end test (nightly/gated). CI
pipeline updates for all build targets.

## Out of Scope

- Real payment processing (handled by PARKING_OPERATOR_ADAPTOR, outside this repo)
- PARKING_APP, COMPANION_APP, and UPDATE_SERVICE (separate repos)
- CLOUD_GATEWAY server-side (separate repo)
- Production-grade security/encryption beyond bearer tokens
- Real GPS or location detection
- Multi-vehicle signal isolation (single VIN for demo)
- Dual-core (M7/M4) partitioning on the Nucleo (M7 only for this spike)

## References

- [Parking Fee Service PRD](https://github.com/rhadp/parking-fee-service/blob/develop/.specs/prd.md)
- [Eclipse Kuksa Databroker](https://github.com/eclipse-kuksa/kuksa-databroker)
- [COVESA Vehicle Signal Specification v5.1](https://covesa.github.io/vehicle_signal_specification/)
- [NATS](https://nats.io)
- [nats.c client](https://github.com/nats-io/nats.c)
- [Red Hat Automotive Stream Distribution](https://github.com/automotive-stream-distribution)
- [ADR-006: Hexagonal Architecture for Portability](../docs/decisions/006-hexagonal-portability.md)
