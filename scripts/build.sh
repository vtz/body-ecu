#!/usr/bin/env bash
set -euo pipefail

BOARD="${1:-native_sim}"

echo "=== Building Body ECU for ${BOARD} ==="
west build -b "${BOARD}" app -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
echo "=== Build complete ==="
