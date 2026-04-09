#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
RESC_FILE="${PROJECT_ROOT}/renode/body_ecu.resc"

echo "=== Running Renode smoke test ==="

renode --disable-xwt --console \
    -e "include @${RESC_FILE}" \
    -e "start" \
    -e "sleep 5" \
    -e "quit"

echo "=== Renode smoke test complete ==="
