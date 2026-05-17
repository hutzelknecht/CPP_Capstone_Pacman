#!/usr/bin/env bash
# build-linux-static.sh – build a static Linux executable inside Docker and
# copy it to ./build/BobMan-linux.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="${SCRIPT_DIR}/build"
IMAGE_TAG="bobman-linux-static-builder"
OUTPUT="${DIST_DIR}/BobMan-linux"

cd "${SCRIPT_DIR}"

echo "==> Building Docker image (this may take a few minutes on first run)…"
docker build \
    -f Dockerfile.linux-static \
    --target artifact \
    -t "${IMAGE_TAG}" \
    .

echo "==> Extracting binary to ${OUTPUT}…"
mkdir -p "${DIST_DIR}"
docker run --rm "${IMAGE_TAG}" cat /BobMan > "${OUTPUT}"
chmod +x "${OUTPUT}"

echo ""
echo "Done! Binary written to: ${OUTPUT}"
echo "Library dependencies:"
file "${OUTPUT}"
