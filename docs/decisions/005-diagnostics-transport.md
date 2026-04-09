# ADR-005: Diagnostics Transport

## Status

Accepted

## Context

UDS diagnostic services need to be reachable over two transports:

1. **DoIP** (Diagnostics over IP) via Ethernet/TCP
2. **DoCAN** (Diagnostics over CAN) via CAN-FD

## Decision

Leverage **OpenBSW's existing diagnostic stack**:

- `DoIpServerSystem` -- ready-made `AsyncLifecycleComponent` for DoIP
  (TCP server, UDP vehicle identification, routing activation, up to 5
  concurrent connections).
- `DoCanSystem` -- DoCAN transport over CAN-FD.
- `TransportRouter` dispatches incoming diagnostic requests to the
  `UdsServiceHandler` regardless of transport.

### UDS Services

Implemented in `libs/platform/diagnostics/`:

| Service | SID | Description |
|---------|-----|-------------|
| DiagnosticSessionControl | 0x10 | Switch between default and extended session |
| ReadDataByIdentifier | 0x22 | Read light states, door lock, vehicle mode |
| IOControlByIdentifier | 0x2F | Control LEDs (extended session only) |
| ReadDTCInformation | 0x19 | Report stored diagnostic trouble codes |

Body services implement `IDiagDataProvider` from `ports/` so the
diagnostics framework reads ECU state through an abstract interface.
This keeps diagnostics decoupled from body domain logic.

## Consequences

- Same UDS services reachable over both Ethernet and CAN-FD.
- Diagnostics are testable on host with `MockDiagDataProvider`.
- Adding new DIDs only requires implementing `IDiagDataProvider` in the
  relevant domain module.
- IOControl is restricted to extended diagnostic session for safety.
