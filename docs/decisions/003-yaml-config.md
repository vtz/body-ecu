# ADR-003: YAML Configuration

## Status

Accepted

## Context

Service IDs, method IDs, event IDs, and CAN gateway mappings must be
configurable. Options:

1. Hard-coded constants
2. JSON configuration files
3. YAML configuration files (opensomeip-examples pattern)
4. Compile-time code generation from a schema

## Decision

Use **YAML files** under `config/` following the opensomeip-examples pattern.

- `config/services.yaml` defines SOME/IP service descriptors.
- `config/can_gateway.yaml` defines CAN-to-SOME/IP mappings.
- Hex integer support (e.g., `0x1000`) via custom parsing.
- Environment variable overrides: `BODY_ECU_NETWORK_PORT=30491`.

### Runtime vs. compile-time

- **native_sim / host tests:** YAML loaded from filesystem at runtime.
- **Embedded targets:** YAML content compiled into the binary as static
  configuration (constexpr or code-generated). This avoids filesystem
  dependencies on the target.

## Consequences

- Familiar format for automotive engineers.
- Easy to diff and version-control.
- yaml-cpp dependency for host builds (FetchContent in CMake).
- Embedded builds need a code-generation step or static initialization.
