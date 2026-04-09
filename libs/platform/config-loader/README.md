# config-loader -- YAML Configuration Parser

Parses `config/services.yaml` and `config/can_gateway.yaml` into typed C++ data structures.

## Features

- Hex integer parsing (`0x1000` style, following opensomeip-examples pattern)
- Environment variable overrides (`BODY_ECU_<SECTION>_<KEY>`)
- Service descriptor, method, event, and eventgroup extraction
- Gateway mapping with direction (SOME/IP-to-CAN, CAN-to-SOME/IP)

## Dependencies

- `yaml-cpp` (fetched via CMake FetchContent in test builds)

## Testing

Unit tests cover YAML parsing, hex integers, env var overrides, missing keys, malformed input, and gateway config parsing.

```bash
ctest -R config_loader_test
```
