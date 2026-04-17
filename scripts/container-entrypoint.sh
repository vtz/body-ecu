#!/usr/bin/env bash
set -euo pipefail
cd /workdir
exec "${@:-/bin/bash}"
