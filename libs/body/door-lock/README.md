# door-lock -- Door Lock Service

Domain logic for door lock control with state machine (Locked / Unlocked / Error).

## SOME/IP Interface

| Method/Event | ID | Description |
|--------------|-----|-------------|
| `Lock` | 0x0001 | Lock the door |
| `Unlock` | 0x0002 | Unlock the door |
| `GetStatus` | 0x0003 | Query current lock state |
| `LockStateChanged` | 0x8001 | Event published on state transitions |

## State Machine

```
Unlocked --Lock()--> Locked
Locked --Unlock()--> Unlocked
* --setError()--> Error
```

## Port Dependencies

- `IGpioPort` -- drives the lock actuator GPIO
- `IButtonInput` -- on-board user button toggles lock state
- `ISomeIpService` -- registers SOME/IP methods and publishes events
- Implements `IModeObserver` -- auto-locks when vehicle mode changes to Run
- Implements `IDiagDataProvider` -- exposes lock state for UDS (DID 0xF101)

## Testing

```bash
ctest -R door_lock_test
```
