#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE_NAME="body-ecu-rpm-build"
TARGET="${1:-hpc}"
ARCH="${2:-$(uname -m)}"
OUTPUT_DIR="${PROJECT_ROOT}/build/rpm"
REPO_ROOT_DIR="${PROJECT_ROOT}/build/rpm-repo"

if [[ "${TARGET}" == "aarch64" || "${TARGET}" == "x86_64" ]]; then
    # Backwards-compatible mode: previous script accepted [arch] only.
    ARCH="${TARGET}"
    TARGET="hpc"
fi

case "${TARGET}" in
    hpc)
        PACKAGE_NAME="body-ecu-hpc"
        VERSION="0.2.0"
        SPEC_FILE="packaging/body-ecu-hpc.spec"
        ;;
    mpu-hostlike)
        PACKAGE_NAME="body-ecu-mpu-hostlike"
        VERSION="0.1.0"
        SPEC_FILE="packaging/body-ecu-mpu-hostlike.spec"
        ;;
    *)
        echo "Unsupported target '${TARGET}'. Use one of: hpc, mpu-hostlike"
        exit 1
        ;;
esac

REPO_DIR="${REPO_ROOT_DIR}/${TARGET}"

mkdir -p "$OUTPUT_DIR"
mkdir -p "$REPO_DIR"

echo "=== Building ${IMAGE_NAME} container (${ARCH}) ==="
podman build \
    --platform "linux/${ARCH}" \
    -t "${IMAGE_NAME}:${ARCH}" \
    -f "${SCRIPT_DIR}/Containerfile.rpm-build" \
    "${PROJECT_ROOT}"

echo "=== Building RPM target '${TARGET}' for ${ARCH} ==="
podman run --rm \
    --platform "linux/${ARCH}" \
    -v "${PROJECT_ROOT}:/src:Z" \
    -v "${OUTPUT_DIR}:/output:Z" \
    -v "${REPO_DIR}:/repo:Z" \
    "${IMAGE_NAME}:${ARCH}" \
    -c '
        VERSION='"${VERSION}"'
        PACKAGE_NAME='"${PACKAGE_NAME}"'
        SPEC_FILE='"${SPEC_FILE}"'
        TARBALL="${PACKAGE_NAME}-${VERSION}.tar.gz"

        tar czf ~/rpmbuild/SOURCES/${TARBALL} \
            --transform "s,^\.,${PACKAGE_NAME}-${VERSION}," \
            --exclude=build --exclude=.git .

        rpmbuild -ba ${SPEC_FILE}

        cp ~/rpmbuild/RPMS/*/${PACKAGE_NAME}-*.rpm /output/
        cp ~/rpmbuild/RPMS/*/${PACKAGE_NAME}-*.rpm /repo/
        cp ~/rpmbuild/SRPMS/${PACKAGE_NAME}-*.rpm /output/
        createrepo_c --update /repo

        echo
        echo "=== Built packages ==="
        ls -1 /output/${PACKAGE_NAME}-*.rpm
        echo
        echo "=== Repo metadata ==="
        ls -1 /repo/repodata
    '

echo
echo "RPMs written to: ${OUTPUT_DIR}/"
echo "Local repo written to: ${REPO_DIR}/"
