#!/usr/bin/env bash
set -euo pipefail
cd /workdir/body-ecu
exec "${@:-/bin/bash}"
