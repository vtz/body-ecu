# Claude AI Instructions

This file provides instructions for Claude AI when working with the body-ecu
project.

## What This Project Is

Body-ECU is an automotive Electronic Control Unit application that runs on
both a microcontroller (MCU) and a high-performance compute (HPC) Linux
host. The MCU side runs Zephyr RTOS firmware; the HPC side runs a Linux
userland service communicating over CAN bus. Together they implement body
domain functions (lighting, HVAC, doors, etc.) for a software-defined
vehicle.

## Project Structure

```
app/                    Zephyr MCU application
  boards/               Board overlay files
  src/                  MCU application source (C/C++)
  CMakeLists.txt        Zephyr CMake build
  prj.conf              Zephyr Kconfig
  renode.conf            Renode simulation config
libs/
  adapters/             HAL adapters (CAN, GPIO, etc.)
  body/                 Body domain logic (shared MCU/HPC)
  platform/             Platform abstraction layer
platforms/
  autosd/               AutoSD (Automotive SIG) Linux build
  posix/                Single-process POSIX build (Linux)
  posix-mcu/            POSIX MCU shim (Linux simulation of MCU)
  posix-mpu/            POSIX MPU build (Linux HPC application)
proto/                  Protobuf / gRPC service definitions
services/
  companion-app/        Python companion app (cloud gateway)
packaging/
  body-ecu-hpc.spec     RPM spec for the HPC application
  build-rpm.sh          RPM build helper script
tests/
  unit/                 Unit tests (GTest, CMake)
  integration/          Integration tests (pytest, two-process)
config/                 Deployment configs (CAN gateway, VSS, services)
renode/                 Renode simulation scripts
west.yml                Zephyr west manifest
build_matrix.json       CI build matrix
```

## Two Sides: MCU and HPC

| Aspect | MCU (Firmware) | HPC (Linux) |
|--------|---------------|-------------|
| Source | `app/src/` | `platforms/posix-mpu/` |
| Build system | Zephyr / west / CMake | CMake / rpmbuild |
| Output | `.bin`, `.elf`, `.hex` | RPM package |
| Target board | `nucleo_h755zi_q` | AutoSD Linux image |
| Build command | `west build -b nucleo_h755zi_q app` | `cmake -B build -S platforms/posix-mpu && cmake --build build` |
| RPM | N/A | `rpmbuild -ba packaging/body-ecu-hpc.spec` |

Shared logic in `libs/` is compiled into both sides.

## Key Commands

### MCU (Zephyr)

```bash
# Initialize west workspace (first time)
west init -l . && west update

# Build for Nucleo H755ZI-Q
west build -b nucleo_h755zi_q app

# Build for native POSIX simulator
west build -b native_sim/native/64 app
```

### HPC (Linux)

```bash
# Build POSIX MPU (HPC application)
cmake -B build/posix-mpu -S platforms/posix-mpu
cmake --build build/posix-mpu -j$(nproc)

# Build RPM (requires Fedora container or rpmbuild tools)
rpmbuild -ba packaging/body-ecu-hpc.spec
```

### Tests

```bash
# Unit tests
cmake -B build/tests -S tests/unit -DBUILD_TESTS=ON
cmake --build build/tests -j$(nproc)
ctest --test-dir build/tests --output-on-failure

# Integration tests (requires both MCU and MPU POSIX builds)
pytest tests/integration/test_two_process.py \
  --mcu-bin=build/posix-mcu/body_ecu_posix_mcu \
  --mpu-bin=build/posix-mpu/body_ecu_posix_mpu -v
```

## Build and Flash Workflow (Agentic)

When fixing a bug or implementing a feature, follow this end-to-end flow:

1. **Identify the affected side** — check issue labels (`mcu`, `hpc`) and
   file paths to determine if the change is MCU, HPC, or both.

2. **Make the code change** — edit source files, ensure local compilation.

3. **Build with Bob** (cluster build):
   ```bash
   # MCU firmware
   bob build body-ecu-zephyr --local --wait
   bob artifacts body-ecu-zephyr  # get OCI artifact ref

   # HPC RPM
   bob build body-ecu-mpu-rpm --local --wait
   bob artifacts body-ecu-mpu-rpm --download /tmp/body-ecu-rpms/
   ```

4. **Flash to hardware via Jumpstarter**:
   - MCU: `jmp create lease -l board=nucleo_h755zi_q`, then flash the OCI
     artifact via Jumpstarter MCP tools or `jmp shell`.
   - HPC: `caib build --target qemu-autosd --flash --extra-rpms *.rpm`

5. **Verify on hardware** — read serial output (MCU) or SSH (HPC) to
   confirm the fix.

6. **Open PR** — commit on a fix branch, push, create PR with verification
   evidence.

See `.cursor/rules/agentic-build-flash.mdc` for the detailed step-by-step
rule, and the Jumpstarter MCP server (`.cursor/mcp.json`) for hardware
interaction tools.

## Coding Conventions

- C/C++ code follows Zephyr coding style (kernel style, 8-char tabs for
  Zephyr-specific code, 4-space indent for application code).
- Python code (companion app, tests) uses Black formatting.
- Commit messages: imperative mood, reference issue numbers with `Fixes #N`.
- All changes must pass CI (see `.github/workflows/ci.yml`).
