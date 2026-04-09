# diagnostics -- UDS Service Framework

UDS diagnostic service handler implementing ISO 14229 services for the Body ECU.

## Supported UDS Services

| SID | Service | Description |
|-----|---------|-------------|
| 0x10 | DiagnosticSessionControl | Switch between Default and Extended sessions |
| 0x22 | ReadDataByIdentifier | Read light states, door lock status, vehicle mode via DID |
| 0x2F | IOControlByIdentifier | Control actuators (requires Extended session) |
| 0x19 | ReadDTCInformation | Report stored diagnostic trouble codes |

## Components

- `UdsServiceHandler` -- routes requests to the correct SID handler, manages session state
- `DtcStore` -- in-memory DTC storage with set/clear/query operations
- `ITransportLayer` -- abstract interface for diagnostic transport (DoIP, DoCAN)

## Port Dependencies

Uses `IDiagDataProvider` from `libs/platform/ports/` to decouple from body-specific data sources. Domain services (`LightingController`, `DoorLockController`) implement this interface.

## Testing

```bash
ctest -R diagnostics_test
```
