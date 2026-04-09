# ADR-004: CAN Gateway Design

## Status

Accepted

## Context

The Body ECU needs a SOME/IP-to-CAN gateway for bridging Ethernet-based
service communication to legacy CAN networks.

## Decision

Follow the **opensomeip-gateways `IGateway` pattern**, adapted for CAN
instead of MQTT/gRPC:

- `CanGateway` in `libs/platform/can-gateway/` is an ECU-generic,
  reusable module.
- `ServiceMapping` structs define bidirectional mappings between
  SOME/IP service/method IDs and CAN IDs, loaded from YAML config.
- `MessageTranslator` handles serialization between SOME/IP payloads
  and CAN frame bytes.
- Gateway core depends only on `ICanBus` and `ISomeIpService` from
  `ports/` -- no OpenBSW or Zephyr dependencies.

## Consequences

- Bidirectional: SOME/IP-to-CAN (e.g., light commands) and
  CAN-to-SOME/IP (e.g., sensor data published as events).
- YAML-driven configuration for easy mapping changes.
- Fully testable on host with `MockCanBus` and `MockSomeIpService`.
- Reusable for any ECU that needs SOME/IP-CAN bridging.
