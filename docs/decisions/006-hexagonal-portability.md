# ADR-006: Hexagonal Architecture for Portability

## Status

Accepted

## Context

The Body ECU spike must be designed so that:

1. Domain modules can be reused across different ECU types (powertrain,
   chassis, body).
2. Platform-specific code (Zephyr GPIO, OpenBSW lifecycle) is isolated
   and swappable.
3. Software modules can eventually migrate from MCU to HPC (MPU) without
   rewriting business logic.
4. All domain modules are testable on the host without cross-compilation
   or target hardware.

## Decision

Adopt a **hexagonal architecture** (ports & adapters) with three distinct
layers:

```
libs/
├── platform/        ECU-generic, reusable across any ECU
│   ├── ports/       Abstract C++ interfaces (IGpioPort, ICanBus, etc.)
│   ├── config-loader/
│   ├── can-gateway/
│   └── diagnostics/
├── body/            Body ECU domain logic (lighting, door-lock, vehicle-mode)
└── adapters/        Platform-specific implementations
    ├── zephyr/      Zephyr GPIO, CAN, timer adapters
    └── openbsw/     OpenBSW lifecycle wrappers + SomeIpSystem
```

### Rules

- Domain modules (`libs/body/`) depend **only** on port interfaces from
  `libs/platform/ports/`. They never include Zephyr, OpenBSW, or
  OpenSOME/IP headers.
- Port interfaces are pure abstract C++ classes with virtual destructors.
  They have zero dependencies (header-only INTERFACE library).
- Adapters (`libs/adapters/`) implement port interfaces using
  platform-specific APIs. They are the only layer that touches Zephyr or
  OpenBSW directly.
- ECU-generic modules (`libs/platform/`) are reusable for any ECU type.
  They depend on port interfaces, not on body-specific code.

### Mock Testing

Shared mock implementations live in `libs/platform/ports/mock/`. All
mocks implement port interfaces, allowing domain logic to be tested
entirely on the host with GoogleTest. No cross-compilation needed.

## Consequences

- **Cross-ECU reuse:** Take `libs/platform/` as-is for a new ECU
  (e.g., `libs/powertrain/`). Wire the new domain services to the same
  port interfaces.
- **HPC migration:** Replace `libs/adapters/zephyr/` and
  `libs/adapters/openbsw/` with `libs/adapters/linux/` (sysfs, SocketCAN,
  POSIX timers). Domain logic and platform modules compile unchanged.
- **Testability:** Every domain module is testable on macOS/Linux
  with mocked ports. CI runs tests without target hardware.
- **Compile-time enforcement:** CMake dependency graph prevents domain
  modules from accidentally depending on platform APIs.

## Alternatives Considered

- **Flat library structure:** Simpler but mixes domain, platform, and
  adapter code. Makes reuse and migration harder.
- **Full AUTOSAR layering:** Too heavy for a spike. The hexagonal
  approach captures the key benefits (portability, testability) without
  the ceremony.
