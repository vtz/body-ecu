# ADR-007: Signal Bus Abstraction for Cross-Domain Communication

## Status

Accepted

## Context

The parking-fee-service PRD requires body services (LOCKING_SERVICE) to
publish vehicle state to a DATA_BROKER (Eclipse Kuksa Databroker) and
receive commands from a cloud gateway. The system spans two processors:

- **MCU (Nucleo / Zephyr):** Safety-critical body services.
- **MPU (AutoSD / Linux):** DATA_BROKER, cloud connectivity.

Domain logic must remain portable across both targets. Kuksa uses gRPC,
which is heavy for MCU deployment. We need an abstraction that works on
both sides without coupling domain code to Kuksa, gRPC, or any transport.

## Decision

Introduce `ISignalBus` as a new port interface in `libs/platform/ports/`.
It provides VSS-style pub/sub with three operations: `publish`, `subscribe`,
and `get`. Signal paths follow the COVESA VSS convention
(`Vehicle.Cabin.Door.Row1.DriverSide.IsLocked`).

`SignalValue` is a `std::variant<bool, int32_t, float, std::string,
std::vector<uint8_t>>` covering all VSS primitive types without pulling
in protobuf or Kuksa dependencies.

Implementations:

- **MCU:** `LocalSignalBus` -- lightweight in-process `std::map` with
  observer callbacks. No network dependencies.
- **MPU:** `KuksaSignalBusAdapter` -- gRPC client to Kuksa Databroker.
- **POSIX (dev):** `InProcessSignalBus` -- same lightweight store for
  host-based testing.

Cross-processor signal exchange uses SOME/IP (see ADR-008), not the
signal bus directly.

## Consequences

- Domain modules gain an optional `ISignalBus*` dependency (nullable for
  backward compatibility) without any transport coupling.
- Safety constraints (vehicle speed, door ajar) can be read via
  `ISignalBus::get()` using the VSS data model, avoiding a proliferation
  of narrow port interfaces.
- The signal bus is purely local to each processor. Cross-processor
  communication is handled by the `SomeIpKuksaBridge` on the MPU, which
  translates SOME/IP events to Kuksa signals and vice versa.

## Alternatives Considered

- **Direct Kuksa gRPC dependency in domain logic:** Rejected. Couples
  domain code to gRPC/protobuf, breaks MCU portability.
- **Separate port per signal (ILockState, IVehicleSpeed, etc.):** Rejected.
  Would create many narrow interfaces for each new signal. The VSS path
  model scales better.
- **CAN-based signal bridge:** Considered but rejected in favor of SOME/IP
  for MPU-MCU communication (see ADR-008).
