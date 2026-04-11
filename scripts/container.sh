#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="body-ecu-dev"
CONTAINER_NAME="body-ecu"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

usage() {
    cat <<EOF
Usage: $(basename "$0") <command>

Commands:
  build    Build the container image
  shell    Start an interactive shell in the container
  clean    Remove the container and image
EOF
    exit 1
}

do_build() {
    echo "==> Building container image '${IMAGE_NAME}'..."
    podman build -t "$IMAGE_NAME" -f "$PROJECT_DIR/Containerfile" "$PROJECT_DIR"
}

ensure_image() {
    if ! podman image exists "$IMAGE_NAME"; then
        echo "Image '${IMAGE_NAME}' not found — building..."
        do_build
    fi
}

case "${1:-}" in
    build)
        do_build
        ;;
    shell)
        ensure_image
        podman run --rm -it \
            -v "$PROJECT_DIR:/workdir/body-ecu:Z" \
            --name "$CONTAINER_NAME" \
            "$IMAGE_NAME"
        ;;
    clean)
        podman rm -f "$CONTAINER_NAME" 2>/dev/null || true
        podman rmi -f "$IMAGE_NAME" 2>/dev/null || true
        echo "==> Cleaned."
        ;;
    *)
        usage
        ;;
esac
