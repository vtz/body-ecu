# ADR-002: SOME/IP Integration

## Status

Accepted

## Context

Body ECU services need to expose SOME/IP methods and events. The options:

1. A standalone `someip-adapter` module in `libs/platform/` wrapping OpenSOME/IP
2. Domain modules using OpenSOME/IP directly
3. `SomeIpSystem` in `libs/adapters/openbsw/` implementing `ISomeIpService`

## Decision

Option 3: **`SomeIpSystem` in `libs/adapters/openbsw/`** implements `ISomeIpService`
directly using OpenSOME/IP. No standalone `someip-adapter` module.

- OpenSOME/IP is a portable C++17 library that works on Zephyr and POSIX.
  Wrapping it in an additional abstraction layer would duplicate abstraction.
- Domain modules depend on `ISomeIpService` (port interface) for testability
  and swapability.
- `SomeIpSystem` is an `AsyncLifecycleComponent` that manages the OpenSOME/IP
  transport lifecycle (init/run/shutdown).
- `MockSomeIpService` in `libs/platform/ports/mock/` is used for unit testing
  all domain modules.

## Consequences

- Simpler architecture: one fewer module to maintain.
- If migrating to vsomeip on HPC, create `libs/adapters/linux/VsomeIpService`
  implementing the same `ISomeIpService` interface. Domain code is untouched.
- The `ISomeIpService` port interface remains the seam for testing and
  stack swapping.
