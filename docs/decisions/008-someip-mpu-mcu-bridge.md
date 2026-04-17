# ADR-008: SOME/IP as MPU-MCU Bridge Protocol

## Status

Accepted

## Context

The Body ECU deployment spans an MCU (Nucleo / Zephyr) and an MPU
(AutoSD / Linux). These two processors must exchange vehicle state and
commands. Two candidates were evaluated:

1. **CAN-FD:** Already available; the MCU has CAN-FD hardware and the
   MPU can use SocketCAN.
2. **SOME/IP:** Already available; the MCU exposes all body services via
   SOME/IP over Ethernet, and the MPU has full TCP/IP networking.

## Decision

Use **SOME/IP over Ethernet** as the cross-processor protocol. The MCU's
body services already expose SOME/IP methods and events (lighting, door
lock, vehicle mode). The MPU runs a `SomeIpKuksaBridge` that acts as a
SOME/IP client:

- **Event-to-signal:** Subscribes to MCU SOME/IP events (LockStateChanged,
  LightStatusChanged, etc.) and writes corresponding VSS signals to the
  DATA_BROKER (Kuksa) via `ISignalBus`.
- **Signal-to-method:** Watches Kuksa for command signals
  (Vehicle.Command.Door.Lock) and sends SOME/IP method requests (Lock,
  Unlock) to the MCU.

The mapping between SOME/IP IDs and VSS signal paths is defined in
`config/signal_bridge.yaml`.

## Consequences

- **No new protocol on the MCU.** Body services are unchanged; they already
  speak SOME/IP. No `CanSignalBridge` adapter is needed.
- **Single bridge component on the MPU.** `SomeIpKuksaBridge` is the only
  new cross-processor adapter. It is purely an MPU concern.
- **CAN-FD remains available** for the existing SOME/IP-to-CAN gateway
  (translating for external CAN devices) but is not used for MPU-MCU
  signaling.
- **Ethernet dependency.** Both processors must share an Ethernet link.
  The Nucleo H755 has on-board Ethernet (RMII), so no additional hardware
  is required.
- **Dev parity.** The two-process POSIX development setup uses SOME/IP
  over localhost, matching the production topology exactly.

## Alternatives Considered

- **CAN-FD bridge:** Lower overhead but requires a new `CanSignalBridge`
  adapter on the MCU, a new frame format, and mapping config. SOME/IP is
  already in use and avoids duplicating the translation layer.
- **gRPC directly between MCU and MPU:** The MCU does not run a gRPC
  stack. Adding one would be heavy for Zephyr on Cortex-M7.
- **Shared memory / IPC:** Not applicable across two separate boards.
