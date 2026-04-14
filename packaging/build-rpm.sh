#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_NAME="body-ecu-rpm-build"
VERSION="0.1.0"
ARCH="${1:-$(uname -m)}"
OUTPUT_DIR="${PROJECT_ROOT}/build/rpm"

mkdir -p "$OUTPUT_DIR"

echo "=== Building ${IMAGE_NAME} container (${ARCH}) ==="
podman build \
    --platform "linux/${ARCH}" \
    -t "${IMAGE_NAME}:${ARCH}" \
    -f "${SCRIPT_DIR}/Containerfile.rpm-build" \
    "${PROJECT_ROOT}"

echo "=== Building RPM for ${ARCH} ==="
podman run --rm \
    --platform "linux/${ARCH}" \
    -v "${PROJECT_ROOT}:/src:Z" \
    -v "${OUTPUT_DIR}:/output:Z" \
    "${IMAGE_NAME}:${ARCH}" \
    -c '
        VERSION='"${VERSION}"'
        TARBALL="body-ecu-hpc-${VERSION}.tar.gz"

        tar czf ~/rpmbuild/SOURCES/${TARBALL} \
            --transform "s,^\.,body-ecu-hpc-${VERSION}," \
            --exclude=build --exclude=.git .

        rpmbuild -ba packaging/body-ecu-hpc.spec

        cp ~/rpmbuild/RPMS/*/*.rpm /output/
        cp ~/rpmbuild/SRPMS/*.rpm  /output/

        echo
        echo "=== Built packages ==="
        ls -1 /output/*.rpm
    '

echo
echo "RPMs written to: ${OUTPUT_DIR}/"
