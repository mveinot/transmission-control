#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Planetary"
IMAGE_NAME="${PLANETARY_LINUX_BUILDER_IMAGE:-planetary-linux-builder:ubuntu22.04-qt6.12.0}"
BUILD_DIR="${PLANETARY_LINUX_BUILD_DIR:-build-linux-docker}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$ROOT_DIR"

if ! command -v docker >/dev/null 2>&1; then
  echo "Docker CLI is required. Run this script on the configured Linux Docker host."
  exit 1
fi

echo "Building ${APP_NAME} AppImage in Ubuntu 22.04 / Qt 6.12.0 container"
echo "Repo:      $ROOT_DIR"
echo "Build dir: $BUILD_DIR"
echo "Image:     $IMAGE_NAME"

docker build \
  --file docker/linux/Dockerfile \
  --tag "$IMAGE_NAME" \
  docker/linux

docker run --rm \
  --user "$(id -u):$(id -g)" \
  --env HOME=/tmp \
  --env PLANETARY_LINUX_BUILD_DIR="$BUILD_DIR" \
  --env CMAKE_GENERATOR=Ninja \
  --volume "$ROOT_DIR:/workspace" \
  --workdir /workspace \
  "$IMAGE_NAME" \
  bash -euo pipefail -c './scripts/package-linux-appimage.sh'

echo
echo "AppImage build complete. See ${ROOT_DIR}/dist/"
