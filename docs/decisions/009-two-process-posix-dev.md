# ADR-009: Two-Process POSIX Development Environment

## Status

Accepted

## Context

The Body ECU targets two processors: MCU (Nucleo / Zephyr) and MPU
(AutoSD / Linux). Developers need a fast feedback loop on the host
without hardware, VMs, or emulators.

The existing single-process POSIX build (`platforms/posix/`) runs all
services in one binary. This does not exercise the SOME/IP communication
path between MCU and MPU.

## Decision

Provide two separate POSIX build targets that mirror the production
topology:

- **`platforms/posix-mcu/`:** Runs body services (lighting, door lock,
  vehicle mode, CAN gateway, diagnostics) with SOME/IP server on
  `0.0.0.0:30490`. Uses Linux adapters (ConsoleGpio, SocketCAN, stdin
  button) and `InProcessSignalBus`.

- **`platforms/posix-mpu/`:** Runs `SomeIpKuksaBridge` as a SOME/IP
  client connecting to the MCU process, plus `CloudGatewayClient` with
  `InProcessSignalBus` and `StubCloudTransport`.

Both processes communicate over SOME/IP via localhost, exercising the
same protocol path as the production MCU-MPU Ethernet link.

The existing single-process `platforms/posix/` build is retained for
quick local testing where the bridge is not needed.

## Consequences

- Developers test the full MCU-MPU SOME/IP flow on their laptop.
- CI can run a two-process integration test without Renode or QEMU.
- The MPU process can be pointed at a real Nucleo board by changing
  the host argument, enabling mixed host/hardware development.
