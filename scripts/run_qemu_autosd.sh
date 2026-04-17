#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

IMAGE="${1:-autosd.qcow2}"
TAP_QEMU="${TAP_QEMU:-tap-qemu}"
MEMORY="${MEMORY:-2G}"

echo "=== Launching AutoSD VM ==="
echo "Image:  $IMAGE"
echo "TAP:    $TAP_QEMU"
echo "Memory: $MEMORY"
echo ""

qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a57 \
    -m "$MEMORY" \
    -drive "file=$IMAGE,format=qcow2,if=virtio" \
    -netdev "tap,id=net0,ifname=$TAP_QEMU,script=no,downscript=no" \
    -device virtio-net-pci,netdev=net0 \
    -nographic \
    -serial mon:stdio
