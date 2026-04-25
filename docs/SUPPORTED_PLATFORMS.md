# Supported Platforms

## Build Targets

| Platform | Build Command | RTOS / OS | Notes |
|----------|--------------|-----------|-------|
| **POSIX (single-process)** | `cmake -B build/posix -S platforms/posix` | None (pthreads) | All services in one binary, InProcessSignalBus |
| **POSIX MCU (dev)** | `cmake -B build/posix-mcu -S platforms/posix-mcu` | None (pthreads) | Body services + SOME/IP server on `0.0.0.0:30490` |
| **POSIX MPU (dev)** | `cmake -B build/posix-mpu -S platforms/posix-mpu` | None (pthreads) | SomeIpKuksaBridge + CloudGatewayClient, SOME/IP client |
| **AutoSD (MPU)** | `cmake -B build/autosd -S platforms/autosd` | Linux (AutoSD) | Kuksa + NATS adapters, production MPU target |
| **AutoSD (MPU host-like image)** | `packaging/build-rpm.sh mpu-hostlike aarch64` + AIB compose | Linux (AutoSD) | POSIX-MPU runtime model (CLI + SOME/IP + stub cloud) installed via local RPM repo |
| native_sim | `west build -b native_sim app` | Zephyr (POSIX) | Zephyr kernel simulation |
| NUCLEO-H755ZI-Q | `west build -b nucleo_h755zi_q/stm32h755xx/m7 app` | Zephyr | M7 core, Ethernet + CAN-FD |

## Two-Process POSIX Development

Run both processes locally to exercise the full SOME/IP communication path:

```bash
# Terminal 1: MCU process (SOME/IP server)
cmake -B build/posix-mcu -S platforms/posix-mcu && cmake --build build/posix-mcu
./build/posix-mcu/body_ecu_posix_mcu

# Terminal 2: MPU process (SOME/IP client)
cmake -B build/posix-mpu -S platforms/posix-mpu && cmake --build build/posix-mpu
./build/posix-mpu/body_ecu_posix_mpu 127.0.0.1
```

## Platform Adapters

| Platform | Adapter Set | Signal Bus | Cloud Transport |
|----------|------------|------------|-----------------|
| POSIX | `libs/adapters/linux/` | InProcessSignalBus | StubCloudTransport |
| AutoSD | `libs/adapters/autosd/` | KuksaSignalBusAdapter (gRPC) | NatsCloudTransportAdapter |
| Zephyr | `libs/adapters/zephyr/` | LocalSignalBus | N/A |

All platforms share the same domain logic (`libs/body/`), platform modules (`libs/platform/`),
and OpenBSW lifecycle wrappers (`libs/adapters/openbsw/`).

## Emulation

| Platform | Tool | Script | Notes |
|----------|------|--------|-------|
| STM32H753 | [Renode](https://renode.io) | `renode/body_ecu.resc` | Uses nucleo_h753zi.repl (single-core sibling) |
| STM32H753 + AutoSD | Renode + QEMU | `renode/body_ecu_vnet.resc` | TAP bridge for SOME/IP integration |

### QEMU + Renode Virtual Integration

```bash
# 1. Create virtual network (Linux host)
sudo scripts/vnet_setup.sh

# 2. Start Renode with TAP networking
renode renode/body_ecu_vnet.resc

# 3. Start AutoSD VM
scripts/run_qemu_autosd.sh autosd.qcow2

# 4. Clean up
sudo scripts/vnet_teardown.sh
```

## RPM Package (HPC / MPU)

The `body-ecu-hpc` RPM packages the POSIX MPU build for deployment on
Fedora / AutoSD / RHIVOS. It installs the binary, systemd unit, and
config files.

### Build the source tarball and RPM

```bash
# Create the source tarball rpmbuild expects
VERSION=0.1.0
git archive --format=tar.gz --prefix=body-ecu-hpc-${VERSION}/ \
    -o ~/rpmbuild/SOURCES/body-ecu-hpc-${VERSION}.tar.gz HEAD

# Build the RPM (in a Fedora container or VM)
rpmbuild -ba packaging/body-ecu-hpc.spec
```

### Quick build in a Fedora container

```bash
podman run --rm -v $(pwd):/src:Z fedora:latest bash -c '
    dnf install -y rpm-build cmake gcc-c++ git-core systemd-rpm-macros
    VERSION=0.1.0
    mkdir -p ~/rpmbuild/SOURCES
    cd /src
    tar czf ~/rpmbuild/SOURCES/body-ecu-hpc-${VERSION}.tar.gz \
        --transform "s,^,body-ecu-hpc-${VERSION}/," \
        --exclude=build --exclude=.git .
    rpmbuild -ba packaging/body-ecu-hpc.spec
    cp ~/rpmbuild/RPMS/*/*.rpm /src/build/
'
```

### Install and run on the target

```bash
sudo dnf install ./body-ecu-hpc-0.1.0-1.*.rpm
sudo systemctl enable --now body-ecu-hpc

# Override MCU host if needed
sudo systemctl edit body-ecu-hpc  # or edit /etc/body-ecu/body-ecu-hpc.env
```

### Packaging files

| File | Purpose |
|------|---------|
| `packaging/body-ecu-hpc.spec` | RPM spec |
| `packaging/body-ecu-hpc.service` | systemd unit |
| `packaging/body-ecu-hpc.env` | Environment file (MCU host config) |
| `packaging/body-ecu-mpu-hostlike.spec` | Host-like MPU RPM spec (POSIX MPU build) |
| `packaging/aib/body-ecu-mpu-hostlike.aib.yml` | AIB image manifest consuming local RPM repo |

## Cross-Processor Communication

MPU and MCU communicate via SOME/IP over Ethernet (see ADR-008).
The `SomeIpKuksaBridge` on the MPU translates between SOME/IP
events/methods and VSS signals in the Kuksa Databroker (see ADR-007).
