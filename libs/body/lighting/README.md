# lighting -- Exterior Lighting Service

Domain logic for exterior lighting control (headlight, turn signal, brake light).

## SOME/IP Interface

| Method/Event | ID | Description |
|--------------|-----|-------------|
| `SetLightState` | 0x0001 | Set a light on/off by ID |
| `GetLightStatus` | 0x0002 | Query state of all 3 lights |
| `LightStatusChanged` | 0x8001 | Event published on any state transition |

## Port Dependencies

- `IGpioPort` -- controls on-board LEDs (green=headlight, yellow=turn, red=brake)
- `ISomeIpService` -- registers SOME/IP methods and publishes events
- Implements `IModeObserver` -- turns off all lights when vehicle mode goes to Off
- Implements `IDiagDataProvider` -- exposes light state for UDS ReadDataByIdentifier (DID 0xF100)

## Testing

```bash
ctest -R lighting_test
```

Tests cover SetLightState with GPIO verification, GetLightStatus, event publication on transitions, invalid light IDs, and mode change reactions.
