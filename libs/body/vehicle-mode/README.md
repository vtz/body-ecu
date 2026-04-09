# vehicle-mode -- Vehicle Mode Service

Domain logic for vehicle mode management (Off, Accessory, Run, Crank).

## SOME/IP Interface

Uses the SOME/IP **field** pattern with getter, setter, and notifier:

| Method/Event | ID | Description |
|--------------|-----|-------------|
| `getter` | 0x0001 | Get current vehicle mode |
| `setter` | 0x0002 | Set vehicle mode (validates transition) |
| `notifier` | 0x8001 | Event published on mode change |

## Valid Transitions

```
Off --> Accessory
Accessory --> Off, Run
Run --> Accessory, Crank
Crank --> Run
```

## Observer Pattern

`VehicleModeManager` notifies all registered `IModeObserver` instances when the mode changes. `LightingController` and `DoorLockController` implement this interface for cross-service coordination.

## Port Dependencies

- `ISomeIpService` -- field getter/setter/notifier
- `IModeObserver` -- notifies subscribers of mode changes

## Testing

```bash
ctest -R vehicle_mode_test
```
