#!/usr/bin/env bash
set -euo pipefail

VERSION_FILE="version.h"

VERSION=$(grep -E '^#define[[:space:]]+__PLANETARY_VERSION__' "$VERSION_FILE" \
  | sed -E 's/.*"([^"]+)".*/\1/')

if [[ -z "$VERSION" ]]; then
  echo "Could not extract __PLANETARY_VERSION__ from $VERSION_FILE"
  exit 1
fi

TAG="v${VERSION}"

git checkout main
git pull --ff-only

if [[ -n "$(git status --porcelain)" ]]; then
  echo "Working tree is not clean."
  git status --short
  exit 1
fi

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Tag already exists locally: $TAG"
  exit 1
fi

if git ls-remote --tags origin | grep -q "refs/tags/${TAG}$"; then
  echo "Tag already exists on origin: $TAG"
  exit 1
fi

git tag -a "$TAG" -m "Planetary $TAG"
git push origin "$TAG"

echo "Created and pushed $TAG"
