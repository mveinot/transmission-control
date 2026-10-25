#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SSH_TARGET="${PLANETARY_LINUX_SSH_TARGET:-docker}"
REMOTE_REPO="${PLANETARY_LINUX_REPO:-/home/vmark/planetary-release-checkout}"
GIT_URL="${PLANETARY_RELEASE_GIT_URL:-https://github.com/mveinot/transmission-control.git}"

if [[ $# -ne 4 ]]; then
  echo "Usage: $0 <tag> <commit> <run-id> <local-output-dir>" >&2
  exit 2
fi
TAG="$1"
COMMIT="$2"
RUN_ID="$3"
OUTPUT_DIR="$4"

if [[ ! "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+(-beta)?$ ||
      ! "$COMMIT" =~ ^[0-9a-f]{40}$ ||
      ! "$RUN_ID" =~ ^[0-9]{8}T[0-9]{6}Z-[0-9]+$ ||
      "$REMOTE_REPO" =~ [[:space:]] ]]; then
  echo "Invalid release-build parameters." >&2
  exit 2
fi
if [[ "$REMOTE_REPO" != /* ]]; then
  echo "PLANETARY_LINUX_REPO must be an absolute Linux path." >&2
  exit 2
fi

mkdir -p "$OUTPUT_DIR"
echo "Linux host: $SSH_TARGET"
echo "Checkout:   $REMOTE_REPO"
ssh -T -o BatchMode=yes "$SSH_TARGET" bash -s -- \
  "$TAG" "$COMMIT" "$RUN_ID" "$REMOTE_REPO" "$GIT_URL" \
  < "$ROOT_DIR/scripts/build-linux-remote-worker.sh"

VERSION="${TAG#v}"
VERSION="${VERSION%-beta}"
APPIMAGE="$REMOTE_REPO/dist/Planetary-$VERSION-x86_64.AppImage"
scp "$SSH_TARGET:$APPIMAGE" "$OUTPUT_DIR/"
printf 'tag=%s\ncommit=%s\nversion=%s\n' "$TAG" "$COMMIT" "$VERSION" > "$OUTPUT_DIR/Linux-build-info.txt"
echo "AppImage copied to $OUTPUT_DIR"
