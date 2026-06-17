#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Planetary"
DEFAULT_BRANCH="main"
BUILD_DIR="${BUILD_DIR:-build-tag-version}"

ROOT_DIR="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT_DIR"

echo "Preparing release tag for ${APP_NAME}"
echo "Repo: $ROOT_DIR"

if [[ ! -f "VERSION.txt" ]]; then
  echo "Missing VERSION.txt"
  echo "The current CMake versioning expects VERSION.txt at the repo root."
  exit 1
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

if [[ "$CURRENT_BRANCH" != "$DEFAULT_BRANCH" ]]; then
  echo "You are on branch '$CURRENT_BRANCH', not '$DEFAULT_BRANCH'."
  echo "Refusing to tag from the wrong branch, because chaos is already well-funded."
  exit 1
fi

git fetch origin

git pull --ff-only origin "$DEFAULT_BRANCH"

if [[ -n "$(git status --porcelain)" ]]; then
  echo "Working tree is not clean."
  git status --short
  exit 1
fi

rm -rf "$BUILD_DIR"

CMAKE_ARGS=(
  -S "$ROOT_DIR"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE=Release
)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
fi

if [[ -n "${Qt6_DIR:-}" ]]; then
  CMAKE_ARGS+=("-DQt6_DIR=${Qt6_DIR}")
fi

echo "Configuring CMake to generate version metadata..."
cmake "${CMAKE_ARGS[@]}"

VERSION_FILE="${BUILD_DIR}/planetary-version.txt"

if [[ ! -f "$VERSION_FILE" ]]; then
  echo "Missing generated version file: $VERSION_FILE"
  echo "Check that CMake writes planetary-version.txt during configure."
  exit 1
fi

VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"

if [[ -z "$VERSION" ]]; then
  echo "Generated version file is empty: $VERSION_FILE"
  exit 1
fi

if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Generated version does not look like major.minor.patch.build: $VERSION"
  exit 1
fi

TAG="v${VERSION}"

echo "Generated version: $VERSION"
echo "Release tag:       $TAG"

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Tag already exists locally: $TAG"
  exit 1
fi

if git ls-remote --tags origin | grep -q "refs/tags/${TAG}$"; then
  echo "Tag already exists on origin: $TAG"
  exit 1
fi

COMMIT="$(git rev-parse --short HEAD)"

git tag -a "$TAG" -m "${APP_NAME} ${TAG}" -m "Commit: ${COMMIT}"
git push origin "$TAG"

echo
echo "Created and pushed release tag:"
echo "$TAG"
