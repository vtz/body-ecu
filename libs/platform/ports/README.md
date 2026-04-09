# ports -- Abstract Interfaces

Header-only CMake INTERFACE library defining the port interfaces that decouple domain logic from platform-specific implementations.

## Interfaces

| Interface | Purpose |
|-----------|---------|
| `IGpioPort` | Digital output read/write |
| `ICanBus` | CAN frame send/receive with RX callback |
| `ISomeIpService` | SOME/IP method registration, event publication, response sending |
| `IButtonInput` | Button press callback registration |
| `IDiagDataProvider` | UDS diagnostic data read and I/O control |
| `IModeObserver` | Vehicle mode change notifications |
| `ITimerService` | Periodic and one-shot timer scheduling |

## Shared Mocks

`mock/` contains GoogleMock implementations of all port interfaces for use in host-based unit tests. The mocks are exposed as the `port_mocks` CMake INTERFACE library.

## Design Rules

- Ports contain **zero** implementation -- only pure virtual interfaces and lightweight data types (`CanFrame`, `SomeIpMessage`, `DiagData`, etc.).
- Domain modules (`libs/body/`) and platform modules (`libs/platform/`) depend **only** on these interfaces.
- Platform adapters (`libs/adapters/`) implement these interfaces for a specific platform (Zephyr, Linux, etc.).
