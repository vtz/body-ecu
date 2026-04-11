#!/usr/bin/env bash
set -euo pipefail

TARGET="${1:-native_sim}"

case "${TARGET}" in
    posix|linux)
        echo "=== Building Body ECU for POSIX (Linux) ==="
        cmake -B build/posix -S platforms/posix
        cmake --build build/posix -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
        echo "=== Build complete: build/posix/body_ecu_posix ==="
        ;;
    *)
        echo "=== Building Body ECU for ${TARGET} (Zephyr) ==="
        west build -b "${TARGET}" app -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        echo "=== Build complete ==="
        ;;
esac
