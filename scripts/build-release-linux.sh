#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Planetary"
BUILD_DIR="build-release"

ROOT_DIR="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT_DIR"

echo "Building ${APP_NAME} release for Linux"
echo "Repo:      $ROOT_DIR"
echo "Build dir: $BUILD_DIR"

if [[ "${1:-}" == "--clean" ]]; then
  echo "Removing existing build directory..."
  rm -rf "$BUILD_DIR"
fi

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release

if command -v nproc >/dev/null 2>&1; then
  JOBS="$(nproc)"
else
  JOBS="4"
fi

cmake --build "$BUILD_DIR" -j "$JOBS"

echo
echo "Release build complete."

if [[ -f "$BUILD_DIR/planetary-version.txt" ]]; then
  VERSION="$(tr -d '[:space:]' < "$BUILD_DIR/planetary-version.txt")"
  echo "Version: $VERSION"
fi

if [[ -x "$BUILD_DIR/$APP_NAME" ]]; then
  echo "Binary:"
  echo "  $BUILD_DIR/$APP_NAME"
elif [[ -x "$BUILD_DIR/Planetary" ]]; then
  echo "Binary:"
  echo "  $BUILD_DIR/Planetary"
else
  echo "Build output is in:"
  echo "  $BUILD_DIR"
fi
