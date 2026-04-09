# can-gateway -- SOME/IP to CAN Gateway

Bidirectional gateway bridging SOME/IP services and CAN frames, following the opensomeip-gateways `IGateway` pattern.

## Architecture

- `ServiceMapping` defines direction (SOME/IP-to-CAN or CAN-to-SOME/IP) with service/method/event IDs and CAN IDs
- `MessageTranslator` handles payload serialization between SOME/IP and CAN frame formats
- `CanGateway` manages mappings, builds lookup indices, and routes messages in both directions

## Port Dependencies

Depends only on `ICanBus` and `ISomeIpService` from `libs/platform/ports/` -- no platform-specific code.

## Testing

```bash
ctest -R can_gateway_test
```

Tests cover SOME/IP-to-CAN translation, CAN-to-SOME/IP, unmapped message drop, payload serialization, bidirectional routing, mapping configuration, and lifecycle.
