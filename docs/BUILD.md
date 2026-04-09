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
