# Build Instructions

## Prerequisites

- CMake >= 3.20
- Ninja build system
- Zephyr SDK (for cross-compilation)
- Python 3.10+ with `west` installed
- A C++17 compiler (GCC 10+ or Clang 14+)

## Set up the west workspace

```bash
cd body-ecu
west init -l .
west update
```

## POSIX build (Linux/macOS -- no Zephyr required)

The POSIX build produces a standalone executable that runs directly on
a Linux or macOS host. It uses real sockets for SOME/IP, SocketCAN for
CAN, console output for GPIO, and stdin for button input.

```bash
cmake -B build/posix -S platforms/posix
cmake --build build/posix
./build/posix/body_ecu_posix [vcan0]
```

Or using CMake presets:

```bash
cd platforms/posix
cmake --preset posix-debug
cmake --build --preset posix-debug
```

### SocketCAN setup (Linux only)

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

Monitor CAN traffic with `candump vcan0` and inject frames with `cansend vcan0 200#DEADBEEF`.

## Zephyr builds

### native_sim (host simulation)

```bash
west build -b native_sim app
west build -t run
```

### NUCLEO-H755ZI-Q (hardware)

```bash
west build -b nucleo_h755zi_q/stm32h755xx/m7 app
west flash
```

## RPM package (HPC / MPU)

The `body-ecu-hpc` RPM packages the MPU-side binary for deployment on
Fedora / AutoSD / RHIVOS. Requires [Podman](https://podman.io).

### Quick build (native arch)

```bash
packaging/build-rpm.sh
```

### Cross-build for aarch64 (from any host)

```bash
packaging/build-rpm.sh aarch64
```

Both produce binary and source RPMs in `build/rpm/`.

### What the script does

1. Builds a Fedora 41 container with RPM build tools (`packaging/Containerfile.rpm-build`)
2. Creates a source tarball from your working tree
3. Runs `rpmbuild -ba` against `packaging/body-ecu-hpc.spec`
4. Copies the RPMs to `build/rpm/`

### Manual build (without the script)

```bash
# Build the container image
podman build --platform linux/aarch64 \
    -t body-ecu-rpm-build:aarch64 \
    -f packaging/Containerfile.rpm-build .

# Run rpmbuild inside it
podman run --rm --platform linux/aarch64 \
    -v $(pwd):/src:Z \
    body-ecu-rpm-build:aarch64 \
    -c '
        VERSION=0.1.0
        tar czf ~/rpmbuild/SOURCES/body-ecu-hpc-${VERSION}.tar.gz \
            --transform "s,^\.,body-ecu-hpc-${VERSION}," \
            --exclude=build --exclude=.git .
        rpmbuild -ba packaging/body-ecu-hpc.spec
    '
```

### Install on the target

```bash
sudo dnf install ./body-ecu-hpc-0.1.0-1.*.rpm
sudo systemctl enable --now body-ecu-hpc
```

The MCU host address defaults to `192.168.100.10` and can be changed in
`/etc/body-ecu/body-ecu-hpc.env`.

## Host-based unit tests

Unit tests do **not** require Zephyr or a cross-compiler. They build
natively using CMake and GoogleTest.

```bash
cmake -B build/tests -S tests/unit -DBUILD_TESTS=ON
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure --output-junit test-results.xml
```

### With coverage

```bash
cmake -B build/tests -S tests/unit -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_C_FLAGS="--coverage"
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
gcovr --root . --filter 'libs/' --html coverage.html --xml coverage.xml
```
