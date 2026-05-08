# Architecture Overview

## System Context

The Body ECU spans two processors that communicate over Ethernet (SOME/IP):

```
  Cloud (NATS)               MPU (AutoSD / Linux)          MCU (Nucleo / Zephyr)
 ┌──────────────┐           ┌──────────────────────┐      ┌──────────────────────┐
 │ NATS Broker  │◄──NATS───►│ CLOUD_GATEWAY_CLIENT │      │                      │
 │              │           │                      │      │  LOCKING_SERVICE     │
 │ Companion    │           │ Kuksa Databroker     │      │  LIGHTING            │
 │ App          │           │ (DATA_BROKER)        │      │  VEHICLE_MODE        │
 └──────────────┘           │                      │      │  SPEED_SIMULATOR     │
                            │ SomeIpKuksaBridge    │      │  CAN_GATEWAY         │
 ┌──────────────┐           │                      │      │  DIAGNOSTICS         │
 │ DoIP Diag    ├─Ethernet─►│                      │      │                      │
 │ Tool         │           └──────┬───────────────┘      └──────┬───────────────┘
 └──────────────┘                  │    SOME/IP (UDP)            │
                                   └────────────────────────────┘
 ┌──────────────┐                                                │
 │ candump      ├──CAN-FD───────────────────────────────────────┘
 │ (USB-CAN)    │
 └──────────────┘
```

**MCU** (NUCLEO-H755ZI-Q / Zephyr): Runs safety-critical body services.
Exposes all services via SOME/IP server on port 30490. Has no direct
cloud connectivity -- that is handled entirely by the MPU.

**MPU** (AutoSD / Linux): Runs the `SomeIpKuksaBridge` as a SOME/IP client
to the MCU, translating between SOME/IP events/methods and VSS signals in
the Kuksa Databroker. The `CloudGatewayClient` bridges the signal bus to
the cloud via NATS.

## Layered Architecture

The software follows a hexagonal (ports & adapters) architecture:

| Layer | Path | Purpose |
|-------|------|---------|
| **Port Interfaces** | `libs/platform/ports/` | Abstract C++ interfaces with zero dependencies |
| **Domain Logic** | `libs/body/` | Portable business logic (lighting, door-lock, vehicle-mode, speed-simulator) |
| **ECU-Generic** | `libs/platform/` | Reusable modules (config-loader, can-gateway, diagnostics, cloud-gateway) |
| **Adapters** | `libs/adapters/` | Platform-specific implementations (Zephyr, Linux, AutoSD) |
| **Application** | `app/`, `platforms/` | Entry points and wiring per target |

### Dependency Rule

Dependencies flow **inward** only:

```
Adapters ──► Port Interfaces ◄── Domain Logic
                    ▲
                    │
              ECU-Generic
```

Domain modules never depend on adapters, Zephyr, AutoSD, or external libraries.

### Platform Adapter Sets

| Platform | Adapter Path | Signal Bus | Cloud Transport |
|----------|-------------|------------|-----------------|
| Zephyr (MCU) | `libs/adapters/zephyr/` | `LocalSignalBus` | N/A |
| AutoSD (MPU) | `libs/adapters/autosd/` | `KuksaSignalBusAdapter` (gRPC) | `NatsCloudTransportAdapter` |
| POSIX (dev) | `libs/adapters/linux/` | `InProcessSignalBus` | `StubCloudTransport` |

## Key Services

### MCU -- Body Domain

- **Exterior Lighting:** SOME/IP methods SetLightState / GetLightStatus,
  event LightStatusChanged. Controls on-board LEDs via IGpioPort.
- **Door Lock:** State machine (Locked/Unlocked/Error), SOME/IP
  Lock/Unlock/GetStatus, button toggle via IButtonInput. Subscribes to
  `Vehicle.Command.Door.Lock` via ISignalBus for cloud-originated commands.
  Validates safety constraints (speed, door ajar) before actuation.
- **Vehicle Mode:** SOME/IP field with getter/setter/notifier
  (Off/Accessory/Run/Crank). Cross-service mode notifications via
  IModeObserver.
- **Speed Simulator:** Reads a 10k potentiometer via IAdcInput (ADC
  on PA3/A0), converts to vehicle speed using a simple physics model
  (acceleration/drag). Broadcasts speed every 100ms via SOME/IP
  event (service 0x1003) and publishes `Vehicle.Speed` to ISignalBus.
  On POSIX, reads throttle from `/tmp/body_ecu_throttle`.

### MCU -- Architectural Showcases

- **SOME/IP-to-CAN Gateway:** Bidirectional translation driven by YAML
  mappings in `config/can_gateway.yaml`.
- **UDS Diagnostics:** ReadDataByID (0x22), IOControl (0x2F),
  ReadDTC (0x19) over DoIP (Ethernet) and DoCAN (CAN-FD).

### MPU -- Cloud Connectivity

- **SomeIpKuksaBridge:** SOME/IP client that translates MCU events into
  VSS signals on the Kuksa Databroker and vice versa. Mapping defined
  in `config/signal_bridge.yaml`.
- **CloudGatewayClient:** Bridges NATS cloud commands to the signal bus.
  Subscribes to `vehicles.{vin}.command.door.lock` on NATS, publishes
  lock state changes and command responses back to the cloud.
- **Kuksa Databroker:** Eclipse Kuksa (deployed separately). Provides
  VSS-compliant gRPC pub/sub for vehicle signals.

## Cross-Processor Communication

MCU and MPU communicate exclusively via SOME/IP over Ethernet (see
[ADR-008](../decisions/008-someip-mpu-mcu-bridge.md)). The MCU does not
run gRPC or NATS -- those are purely MPU concerns.

## Configuration

Service IDs, method IDs, and CAN mappings are defined in YAML files
under `config/`. See `config/services.yaml`, `config/can_gateway.yaml`,
`config/signal_bridge.yaml`, and `config/deployment.yaml`.
