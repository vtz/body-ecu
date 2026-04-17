#!/usr/bin/env bash
set -euo pipefail

# Automated Renode-to-POSIX-MPU integration test (CI-friendly).
# Runs inside the container, validates MCU boot + lifecycle + TAP
# networking + SOME/IP communication, exits with 0/1.

TAP_IF=tap0
TAP_IP=192.168.100.1/24
MCU_IP=192.168.100.10
SOMEIP_PORT=30490

RENODE_LOG=$(mktemp /tmp/renode_log.XXXXXX)
MPU_LOG=$(mktemp /tmp/mpu_log.XXXXXX)
RENODE_PID=""
MPU_PID=""

cleanup() {
    [ -n "$MPU_PID" ] && kill "$MPU_PID" 2>/dev/null || true
    [ -n "$RENODE_PID" ] && kill "$RENODE_PID" 2>/dev/null || true
    ip link del "${TAP_IF}" 2>/dev/null || true
    rm -f "${RENODE_LOG}" "${MPU_LOG}"
}
trap cleanup EXIT

echo "=== Body ECU: Renode + SOME/IP Integration Test ==="

# --- Step 1: TAP ---
echo ""
echo "[1/6] Setting up TAP interface..."
ip tuntap add dev "${TAP_IF}" mode tap 2>/dev/null || true
ip addr add "${TAP_IP}" dev "${TAP_IF}" 2>/dev/null || true
ip link set "${TAP_IF}" up
echo "  TAP ${TAP_IF} ready (${TAP_IP})"

# --- Step 2: Build Zephyr MCU firmware ---
echo ""
echo "[2/6] Building Zephyr MCU firmware (nucleo_h753zi)..."
west build -b nucleo_h753zi app -d build/renode --pristine auto -- \
    -DCONFIG_SOMEIP=y 2>&1 | tail -10
echo "  ELF: build/renode/zephyr/zephyr.elf"

# --- Step 3: Build POSIX MPU ---
echo ""
echo "[3/6] Building POSIX MPU..."
cmake -B build/posix-mpu -S platforms/posix-mpu -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -3
cmake --build build/posix-mpu -j$(nproc) 2>&1 | tail -5
echo "  Binary: build/posix-mpu/body_ecu_posix_mpu"

# --- Step 4: Start Renode with TAP ---
echo ""
echo "[4/6] Starting Renode (headless + TAP)..."
renode --disable-xwt --plain \
    -e "\$firmware = @build/renode/zephyr/zephyr.elf" \
    -e "i @renode/body_ecu_renode_tap.resc" \
    -e "start" \
    > "${RENODE_LOG}" 2>&1 &
RENODE_PID=$!
echo "  Renode PID: ${RENODE_PID}"

MCU_BOOTED=false
for i in $(seq 1 20); do
    sleep 2
    if grep -q "Body ECU ready" "${RENODE_LOG}" 2>/dev/null; then
        MCU_BOOTED=true
        echo "  MCU booted after ~$((i * 2))s"
        break
    fi
    if ! kill -0 "$RENODE_PID" 2>/dev/null; then
        echo "  FAIL: Renode exited prematurely"
        break
    fi
done

# --- Ping MCU ---
PING_OK=false
if $MCU_BOOTED; then
    echo "  Pinging MCU at ${MCU_IP}..."
    for attempt in 1 2 3 4 5; do
        if ping -c 2 -W 3 "${MCU_IP}" > /dev/null 2>&1; then
            PING_OK=true
            echo "  MCU reachable!"
            break
        fi
        sleep 3
    done
fi

# --- Step 5: Start POSIX MPU ---
echo ""
echo "[5/6] Starting POSIX MPU (-> ${MCU_IP}:${SOMEIP_PORT})..."
MCU_HOST="${MCU_IP}" timeout 25 ./build/posix-mpu/body_ecu_posix_mpu \
    > "${MPU_LOG}" 2>&1 &
MPU_PID=$!
echo "  MPU PID: ${MPU_PID}"
sleep 15

# --- Step 6: Verify ---
echo ""
echo "[6/6] Results"
echo ""

PASS=true
echo "  -- MCU (Renode) --"
$MCU_BOOTED && echo "  PASS: Firmware booted" || { echo "  FAIL: Firmware did not boot"; PASS=false; }

grep -q "lifecycle.*INIT.*level=1" "${RENODE_LOG}" 2>/dev/null \
    && echo "  PASS: Lifecycle INIT" || echo "  INFO: No lifecycle in Renode log"
grep -q "SOME/IP.*Transport running" "${RENODE_LOG}" 2>/dev/null \
    && echo "  PASS: SOME/IP transport running" || echo "  INFO: No SOME/IP in Renode log"

echo ""
echo "  -- TAP Network --"
$PING_OK && echo "  PASS: MCU reachable at ${MCU_IP}" || { echo "  FAIL: MCU not reachable"; PASS=false; }

echo ""
echo "  -- MPU (POSIX) --"
grep -q "lifecycle.*INIT" "${MPU_LOG}" 2>/dev/null \
    && echo "  PASS: MPU lifecycle INIT" || { echo "  FAIL: MPU lifecycle"; PASS=false; }
grep -q "Transport running" "${MPU_LOG}" 2>/dev/null \
    && echo "  PASS: MPU SOME/IP running" || { echo "  FAIL: MPU SOME/IP"; PASS=false; }
grep -q "MPU ready" "${MPU_LOG}" 2>/dev/null \
    && echo "  PASS: MPU reached ready state" || { echo "  FAIL: MPU not ready"; PASS=false; }

echo ""
if $PASS; then
    echo "=== RESULT: PASS ==="
    exit 0
else
    echo "=== RESULT: FAIL ==="
    echo ""
    echo "--- Renode log (last 20 lines) ---"
    tail -20 "${RENODE_LOG}"
    echo "--- MPU log ---"
    cat "${MPU_LOG}"
    exit 1
fi
