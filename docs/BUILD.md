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
