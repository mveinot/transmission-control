#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Planetary"
DEFAULT_BRANCH="main"
VERSION_FILE="PLANETARY_VERSION"

ROOT_DIR="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT_DIR"

echo "Preparing release tag for ${APP_NAME}"
echo "Repo: $ROOT_DIR"

if [[ ! -f "$VERSION_FILE" ]]; then
  echo "Missing $VERSION_FILE"
  echo "Expected a base version like: 0.9.0"
  exit 1
fi

BASE_VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"

if [[ -z "$BASE_VERSION" ]]; then
  echo "$VERSION_FILE is empty"
  exit 1
fi

if [[ ! "$BASE_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "$VERSION_FILE must contain major.minor.patch, e.g. 0.9.0"
  echo "Found: $BASE_VERSION"
  exit 1
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

if [[ "$CURRENT_BRANCH" != "$DEFAULT_BRANCH" ]]; then
  echo "You are on branch '$CURRENT_BRANCH', not '$DEFAULT_BRANCH'."
  echo "Refusing to tag from the wrong branch."
  exit 1
fi

git fetch origin
git pull --ff-only origin "$DEFAULT_BRANCH"

if [[ -n "$(git status --porcelain)" ]]; then
  echo "Working tree is not clean."
  git status --short
  exit 1
fi

BUILD_NUMBER="$(git rev-list --count HEAD)"

if [[ -z "$BUILD_NUMBER" || ! "$BUILD_NUMBER" =~ ^[0-9]+$ ]]; then
  echo "Could not determine git commit count."
  exit 1
fi

VERSION="${BASE_VERSION}.${BUILD_NUMBER}"
TAG="v${VERSION}"
COMMIT="$(git rev-parse --short HEAD)"

echo "Base version:  $BASE_VERSION"
echo "Build number:  $BUILD_NUMBER"
echo "Full version:  $VERSION"
echo "Release tag:   $TAG"
echo "Commit:        $COMMIT"

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Tag already exists locally: $TAG"
  exit 1
fi

if git ls-remote --tags origin | grep -q "refs/tags/${TAG}$"; then
  echo "Tag already exists on origin: $TAG"
  exit 1
fi

git tag -a "$TAG" \
  -m "${APP_NAME} ${TAG}" \
  -m "Commit: ${COMMIT}"

git push origin "$TAG"

echo
echo "Created and pushed release tag:"
echo "$TAG"
