# system -- Lifecycle System Adapters

System wrappers that implement `lifecycle::ILifecycleComponent` and bridge
domain controllers with the `LifecycleManager`.

## System Wrappers

Each system owns a domain controller and manages its init/run/shutdown lifecycle:

| System | Domain Controller | Description |
|--------|------------------|-------------|
| `SomeIpSystem` | -- | SOME/IP transport (implements `ISomeIpService`) |
| `LightingSystem` | `LightingController` | Exterior lighting lifecycle |
| `DoorLockSystem` | `DoorLockController` | Door lock lifecycle |
| `VehicleModeSystem` | `VehicleModeManager` | Vehicle mode lifecycle |
| `CanGatewaySystem` | `CanGateway` | CAN gateway lifecycle |
| `DiagnosticsSystem` | `UdsServiceHandler` | UDS diagnostics lifecycle |

## Transport Adapters

| Transport | Interface | Protocol |
|-----------|-----------|----------|
| `DoIpTransport` | `ITransportLayer` | DoIP over TCP (port 13400) |
| `DoCanTransport` | `ITransportLayer` | DoCAN over CAN-FD (ISO-TP) |

Both transports implement the same `ITransportLayer` interface, enabling the same UDS services to be reachable over Ethernet and CAN-FD simultaneously.

## Testing

```bash
ctest -R someip_system_test
ctest -R system_wrappers_test
ctest -R transport_test
```
