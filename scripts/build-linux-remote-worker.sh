#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 5 ]]; then
  echo "Internal error: expected tag, commit, run ID, repository path, and Git URL." >&2
  exit 2
fi
TAG="$1"
EXPECTED_COMMIT="$2"
RUN_ID="$3"
REPO="$4"
GIT_URL="$5"

if [[ ! "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+(-beta)?$ ||
      ! "$EXPECTED_COMMIT" =~ ^[0-9a-f]{40}$ ||
      ! "$RUN_ID" =~ ^[0-9]{8}T[0-9]{6}Z-[0-9]+$ ]]; then
  echo "Invalid release-build parameters." >&2
  exit 2
fi
if [[ ! -e "$REPO" ]]; then
  mkdir -p "$(dirname "$REPO")"
  git clone --branch main --single-branch "$GIT_URL" "$REPO"
elif [[ ! -d "$REPO/.git" ]]; then
  echo "Linux checkout path exists but is not a Git repository: $REPO" >&2
  exit 1
fi

if [[ -n "$(git -C "$REPO" status --porcelain)" ]]; then
  echo "Linux checkout is dirty; refusing to change it: $REPO" >&2
  git -C "$REPO" status --short >&2
  exit 1
fi
if [[ "$(git -C "$REPO" branch --show-current)" != "main" ]]; then
  echo "Linux checkout must be on main before release builds: $REPO" >&2
  exit 1
fi

mkdir -p "$REPO/dist"
exec 9>"$REPO/dist/.release-orchestrator.lock"
if ! flock -n 9; then
  echo "Another Linux release build is already using $REPO." >&2
  exit 1
fi

git -C "$REPO" fetch --prune --tags origin
git -C "$REPO" pull --ff-only origin main
REMOTE_TAG_COMMIT="$(git -C "$REPO" rev-parse "$TAG^{commit}")"
if [[ "$REMOTE_TAG_COMMIT" != "$EXPECTED_COMMIT" ]]; then
  echo "Linux tag $TAG resolves to $REMOTE_TAG_COMMIT; expected $EXPECTED_COMMIT." >&2
  exit 1
fi
if ! git -C "$REPO" merge-base --is-ancestor "$EXPECTED_COMMIT" origin/main; then
  echo "Linux tag $TAG is not on origin/main." >&2
  exit 1
fi

git -C "$REPO" checkout --quiet --detach "$EXPECTED_COMMIT"
restore_main() {
  git -C "$REPO" checkout --quiet main
}
trap restore_main EXIT

echo "Building Linux tag $TAG ($EXPECTED_COMMIT) using Docker..."
"$REPO/scripts/build-release-linux-docker.sh"

VERSION="${TAG#v}"
VERSION="${VERSION%-beta}"
APPIMAGE="$REPO/dist/Planetary-$VERSION-x86_64.AppImage"
if [[ ! -f "$APPIMAGE" ]]; then
  echo "Expected AppImage is missing: $APPIMAGE" >&2
  exit 1
fi
echo "Linux AppImage ready: $APPIMAGE (run $RUN_ID)"
