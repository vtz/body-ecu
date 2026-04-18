# Architecture Overview

## System Context

The Body ECU runs on a NUCLEO-H755ZI-Q board and communicates with
external tools over Ethernet (SOME/IP, DoIP) and CAN-FD (DoCAN).

```
┌─────────────────┐     Ethernet      ┌──────────────────────┐
│  SOME/IP Client ├───────────────────►│                      │
│  (Python/C++)   │◄───────────────────│                      │
└─────────────────┘                    │                      │
                                       │    Body ECU          │
┌─────────────────┐     Ethernet       │    NUCLEO-H755ZI-Q   │
│  DoIP Diag Tool ├───────────────────►│                      │
│                 │◄───────────────────│                      │
└─────────────────┘                    │                      │
                                       │                      │
┌─────────────────┐     CAN-FD         │                      │
│  candump        ├───────────────────►│                      │
│  (USB-CAN)      │◄───────────────────│                      │
└─────────────────┘                    └──────────────────────┘
```

## Layered Architecture

The software follows a hexagonal (ports & adapters) architecture:

| Layer | Path | Purpose |
|-------|------|---------|
| **Port Interfaces** | `libs/platform/ports/` | Abstract C++ interfaces with zero dependencies |
| **Domain Logic** | `libs/body/` | Portable business logic (lighting, door-lock, vehicle-mode, speed-simulator) |
| **ECU-Generic** | `libs/platform/` | Reusable modules (config-loader, can-gateway, diagnostics) |
| **Adapters** | `libs/adapters/` | Platform-specific implementations (Zephyr, OpenBSW) |
| **Application** | `app/` | Zephyr entry point and wiring |

### Dependency Rule

Dependencies flow **inward** only:

```
Adapters ──► Port Interfaces ◄── Domain Logic
                    ▲
                    │
              ECU-Generic
```

Domain modules never depend on adapters, Zephyr, or OpenBSW.

## Key Services

### Tier 1 -- Body Domain

- **Exterior Lighting:** SOME/IP methods SetLightState / GetLightStatus,
  event LightStatusChanged. Controls on-board LEDs via IGpioPort.
- **Door Lock:** State machine (Locked/Unlocked/Error), SOME/IP
  Lock/Unlock/GetStatus, button toggle via IButtonInput.
- **Vehicle Mode:** SOME/IP field with getter/setter/notifier
  (Off/Accessory/Run/Crank). Cross-service mode notifications via
  IModeObserver.
- **Speed Simulator:** Reads a 10k potentiometer via IAdcInput (ADC
  on PA3/A0), converts to vehicle speed using a simple physics model
  (acceleration/drag). Broadcasts speed every 100ms via SOME/IP
  event (service 0x1003) and publishes `Vehicle.Speed` to ISignalBus.
  On POSIX, reads throttle from `/tmp/body_ecu_throttle`.

### Tier 3 -- Architectural Showcases

- **SOME/IP-to-CAN Gateway:** Bidirectional translation driven by YAML
  mappings in `config/can_gateway.yaml`.
- **UDS Diagnostics:** ReadDataByID (0x22), IOControl (0x2F),
  ReadDTC (0x19) over DoIP (Ethernet) and DoCAN (CAN-FD).

## Configuration

Service IDs, method IDs, and CAN mappings are defined in YAML files
under `config/`. See `config/services.yaml` and `config/can_gateway.yaml`.
