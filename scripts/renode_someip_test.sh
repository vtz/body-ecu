#!/usr/bin/env bash
set -euo pipefail

# Renode-to-POSIX-MPU integration test.
# Run this INSIDE the container:
#   docker compose -f docker-compose.renode.yml run body-ecu bash
#   ./scripts/renode_someip_test.sh
#
# What it does:
#   1. Sets up a TAP interface (tap0) with IP 192.168.100.1/24
#   2. Builds the Zephyr MCU firmware (with opensomeip) for nucleo_h753zi
#   3. Builds the POSIX MPU process
#   4. Starts Renode (headless) running the MCU firmware
#   5. Starts the POSIX MPU process connecting to Renode's MCU via SOME/IP
#
# The MCU firmware gets IP 192.168.100.10 (from Zephyr KConfig).
# The host/container side of the TAP is 192.168.100.1.

TAP_IF=tap0
TAP_IP=192.168.100.1/24
MCU_IP=192.168.100.10
SOMEIP_PORT=30490

echo "=== Body ECU: Renode + SOME/IP integration test ==="
echo ""

# --- Step 1: TAP interface ---
echo "[1/5] Setting up TAP interface ${TAP_IF} (${TAP_IP})..."
ip tuntap add dev "${TAP_IF}" mode tap 2>/dev/null || true
ip addr add "${TAP_IP}" dev "${TAP_IF}" 2>/dev/null || true
ip link set "${TAP_IF}" up
echo "  TAP ready."

# --- Step 2: Build Zephyr MCU firmware ---
echo ""
echo "[2/5] Building Zephyr MCU firmware (nucleo_h753zi + opensomeip)..."
west build -b nucleo_h753zi app -d build/renode 2>&1 | tail -5
echo "  Zephyr build done: build/renode/zephyr/zephyr.elf"

# --- Step 3: Build POSIX MPU ---
echo ""
echo "[3/5] Building POSIX MPU process..."
cmake -B build/posix-mpu -S platforms/posix-mpu -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -3
cmake --build build/posix-mpu 2>&1 | tail -3
echo "  POSIX MPU build done: build/posix-mpu/body_ecu_posix_mpu"

# --- Step 4: Start Renode (background, headless) ---
echo ""
echo "[4/5] Starting Renode (headless)..."

RENODE_LOG=$(mktemp /tmp/renode_log.XXXXXX)

renode --disable-xwt --plain \
  -e "\$firmware = @build/renode/zephyr/zephyr.elf; i @renode/body_ecu_renode_tap.resc; start" \
  > "${RENODE_LOG}" 2>&1 &
RENODE_PID=$!

echo "  Renode PID: ${RENODE_PID}"
echo "  Waiting for MCU to boot..."
for i in $(seq 1 20); do
    sleep 2
    if grep -q "Body ECU ready" "${RENODE_LOG}" 2>/dev/null; then
        echo "  MCU booted after ~$((i * 2))s!"
        break
    fi
done

echo "  Checking connectivity to MCU (${MCU_IP})..."
for attempt in 1 2 3; do
    if ping -c 2 -W 3 "${MCU_IP}" > /dev/null 2>&1; then
        echo "  MCU reachable at ${MCU_IP}!"
        break
    fi
    echo "  Ping attempt ${attempt} failed, retrying..."
    sleep 3
done

# --- Step 5: Start POSIX MPU ---
echo ""
echo "[5/5] Starting POSIX MPU (connecting to ${MCU_IP}:${SOMEIP_PORT})..."
echo "  Press Ctrl+C to stop both processes."
echo ""

cleanup() {
    echo ""
    echo "Stopping..."
    kill "${RENODE_PID}" 2>/dev/null || true
    ip link del "${TAP_IF}" 2>/dev/null || true
    rm -f "${RENODE_LOG}"
    echo "Done."
}
trap cleanup EXIT

MCU_HOST="${MCU_IP}" ./build/posix-mpu/body_ecu_posix_mpu
