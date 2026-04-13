#!/usr/bin/env bash
set -euo pipefail

BRIDGE=br-ecu
TAP_RENODE=tap-renode
TAP_QEMU=tap-qemu

echo "=== Tearing down virtual network ==="

sudo ip link set "$TAP_RENODE" down 2>/dev/null || true
sudo ip tuntap del dev "$TAP_RENODE" mode tap 2>/dev/null || true

sudo ip link set "$TAP_QEMU" down 2>/dev/null || true
sudo ip tuntap del dev "$TAP_QEMU" mode tap 2>/dev/null || true

sudo ip link set "$BRIDGE" down 2>/dev/null || true
sudo ip link del "$BRIDGE" type bridge 2>/dev/null || true

echo "Done."
