#!/usr/bin/env bash
set -euo pipefail

BRIDGE=br-ecu
TAP_RENODE=tap-renode
TAP_QEMU=tap-qemu
SUBNET=192.168.100.1/24

echo "=== Creating virtual network for Body ECU ==="

sudo ip link add name "$BRIDGE" type bridge
sudo ip addr add "$SUBNET" dev "$BRIDGE"
sudo ip link set "$BRIDGE" up

sudo ip tuntap add dev "$TAP_RENODE" mode tap
sudo ip link set "$TAP_RENODE" master "$BRIDGE"
sudo ip link set "$TAP_RENODE" up

sudo ip tuntap add dev "$TAP_QEMU" mode tap
sudo ip link set "$TAP_QEMU" master "$BRIDGE"
sudo ip link set "$TAP_QEMU" up

echo ""
echo "Bridge:      $BRIDGE ($SUBNET)"
echo "Renode TAP:  $TAP_RENODE"
echo "QEMU TAP:    $TAP_QEMU"
echo ""
echo "Expected IPs:"
echo "  Renode (MCU):  192.168.100.10"
echo "  QEMU (MPU):    192.168.100.20"
echo "  Host (bridge): 192.168.100.1"
echo ""
echo "Done. Run scripts/vnet_teardown.sh to clean up."
