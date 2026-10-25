#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Planetary"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_ROOT="${PLANETARY_RELEASE_OUTPUT_ROOT:-$ROOT_DIR/dist/release-builds}"
MAC_BUILD_DIR="${PLANETARY_MACOS_BUILD_DIR:-build-macos-universal-release}"
GIT_URL="${PLANETARY_RELEASE_GIT_URL:-https://github.com/mveinot/transmission-control.git}"
BUILD_MACOS=true
BUILD_WINDOWS=true
BUILD_LINUX=true

usage() {
  echo "Usage: $0 [--skip-macos] [--skip-windows] [--skip-linux] <release-tag>"
  echo "Example: $0 --skip-macos --skip-linux v2.6.0.490"
  echo "Builds selected platforms from the exact pushed tag."
}

TAG=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-macos)
      BUILD_MACOS=false
      ;;
    --skip-windows)
      BUILD_WINDOWS=false
      ;;
    --skip-linux)
      BUILD_LINUX=false
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      if [[ $# -ne 1 || -n "$TAG" ]]; then
        usage >&2
        exit 2
      fi
      TAG="$1"
      break
      ;;
    -* )
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      if [[ -n "$TAG" ]]; then
        echo "Only one release tag may be specified." >&2
        usage >&2
        exit 2
      fi
      TAG="$1"
      ;;
  esac
  shift
done

if [[ -z "$TAG" ]]; then
  usage >&2
  exit 2
fi
if [[ "$BUILD_MACOS" == false && "$BUILD_WINDOWS" == false && "$BUILD_LINUX" == false ]]; then
  echo "At least one platform build must be selected." >&2
  exit 2
fi
if [[ ! "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+(-beta)?$ ]]; then
  echo "Invalid release tag: $TAG (expected vX.Y.Z.B or vX.Y.Z.B-beta)." >&2
  exit 2
fi
if [[ "$BUILD_MACOS" == true && ( "$MAC_BUILD_DIR" == /* || "$MAC_BUILD_DIR" == *..* ) ]]; then
  echo "PLANETARY_MACOS_BUILD_DIR must be a relative path within the build worktree." >&2
  exit 2
fi

cd "$ROOT_DIR"
if [[ "$(git branch --show-current)" != "main" ]]; then
  echo "Run this from the main branch." >&2
  exit 2
fi
if [[ -n "$(git status --porcelain)" ]]; then
  echo "Working tree must be clean before building a release." >&2
  git status --short >&2
  exit 2
fi

echo "Fetching main and tags from GitHub over HTTPS..."
git fetch --tags "$GIT_URL" '+refs/heads/main:refs/remotes/origin/main'
if ! git merge-base --is-ancestor HEAD origin/main ||
   ! git merge-base --is-ancestor origin/main HEAD; then
  echo "Local main must be up to date with origin/main; fast-forward it first." >&2
  exit 1
fi

remote_tag_output="$(git ls-remote --tags "$GIT_URL" "refs/tags/$TAG" "refs/tags/$TAG^{}")"
if [[ -z "$remote_tag_output" ]]; then
  echo "Release tag is not present on origin: $TAG" >&2
  exit 1
fi
remote_tag_commit="$(awk '$2 ~ /\^\{\}$/ { print $1; found = 1 } END { if (!found) exit 1 }' <<<"$remote_tag_output" || true)"
if [[ -z "$remote_tag_commit" ]]; then
  remote_tag_commit="$(awk -v ref="refs/tags/$TAG" '$2 == ref { print $1; exit }' <<<"$remote_tag_output")"
fi
if [[ -z "$remote_tag_commit" ]]; then
  echo "Could not resolve the remote tag commit for $TAG." >&2
  exit 1
fi
git fetch --no-tags "$GIT_URL" "refs/tags/$TAG:refs/tags/$TAG"
tag_commit="$(git rev-parse "$TAG^{commit}")"
if [[ "$tag_commit" != "$remote_tag_commit" ]]; then
  echo "Local and remote $TAG point to different commits." >&2
  exit 1
fi
if ! git merge-base --is-ancestor "$tag_commit" origin/main; then
  echo "Tag $TAG is not on origin/main; refusing to build it." >&2
  exit 1
fi

VERSION="${TAG#v}"
VERSION="${VERSION%-beta}"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)-$$"
OUTPUT_DIR="$OUTPUT_ROOT/$TAG/$RUN_ID"
if [[ -e "$OUTPUT_DIR" ]]; then
  echo "Output directory already exists: $OUTPUT_DIR" >&2
  exit 1
fi
mkdir -p "$OUTPUT_DIR/logs"
OUTPUT_DIRS=()
SELECTED_PLATFORMS=()
if [[ "$BUILD_MACOS" == true ]]; then
  mkdir -p "$OUTPUT_DIR/macos"
  OUTPUT_DIRS+=("$OUTPUT_DIR/macos")
  SELECTED_PLATFORMS+=(macOS)
fi
if [[ "$BUILD_WINDOWS" == true ]]; then
  mkdir -p "$OUTPUT_DIR/windows"
  OUTPUT_DIRS+=("$OUTPUT_DIR/windows")
  SELECTED_PLATFORMS+=(Windows)
fi
if [[ "$BUILD_LINUX" == true ]]; then
  mkdir -p "$OUTPUT_DIR/linux"
  OUTPUT_DIRS+=("$OUTPUT_DIR/linux")
  SELECTED_PLATFORMS+=(Linux)
fi

WORKTREE_TMP=""
WORKTREE=""
cleanup() {
  if [[ -n "$WORKTREE" && -d "$WORKTREE" ]]; then
    git -C "$ROOT_DIR" worktree remove --force "$WORKTREE" >/dev/null 2>&1 || true
  fi
  if [[ -n "$WORKTREE_TMP" && -d "$WORKTREE_TMP" ]]; then
    rm -rf "$WORKTREE_TMP"
  fi
}
trap cleanup EXIT

echo "Release tag: $TAG"
echo "Commit:      $tag_commit"
echo "Version:     $VERSION"
echo "Output:      $OUTPUT_DIR"
echo "Platforms:   ${SELECTED_PLATFORMS[*]}"

if [[ "$BUILD_MACOS" == true ]]; then
  WORKTREE_TMP="$(mktemp -d "${TMPDIR:-/tmp}/planetary-release.XXXXXX")"
  WORKTREE="$WORKTREE_TMP/source"
  git worktree add --detach "$WORKTREE" "$tag_commit"

  echo "Building macOS universal DMG..."
  (
    cd "$WORKTREE"
    BUILD_DIR="$MAC_BUILD_DIR" ./scripts/package-macos.sh
  ) 2>&1 | tee "$OUTPUT_DIR/logs/macos.log"

  MAC_VERSION_FILE="$WORKTREE/$MAC_BUILD_DIR/planetary-version.txt"
  MAC_DMG="$WORKTREE/$MAC_BUILD_DIR/$APP_NAME-$VERSION-macOS-universal.dmg"
  if [[ ! -f "$MAC_VERSION_FILE" || "$(tr -d '[:space:]' < "$MAC_VERSION_FILE")" != "$VERSION" ]]; then
    echo "macOS build version does not match release tag $TAG." >&2
    exit 1
  fi
  if [[ ! -f "$MAC_DMG" ]]; then
    echo "Expected macOS DMG is missing: $MAC_DMG" >&2
    exit 1
  fi
  cp "$MAC_DMG" "$OUTPUT_DIR/macos/"
fi

if [[ "$BUILD_WINDOWS" == true ]]; then
  echo "Building Windows installer over SSH..."
  "$ROOT_DIR/scripts/build-windows-remote.sh" \
    "$TAG" "$tag_commit" "$RUN_ID" "$OUTPUT_DIR/windows" \
    2>&1 | tee "$OUTPUT_DIR/logs/windows.log"
fi

if [[ "$BUILD_LINUX" == true ]]; then
  echo "Building Linux AppImage over SSH..."
  "$ROOT_DIR/scripts/build-linux-remote.sh" \
    "$TAG" "$tag_commit" "$RUN_ID" "$OUTPUT_DIR/linux" \
    2>&1 | tee "$OUTPUT_DIR/logs/linux.log"
fi

printf 'tag=%s\ncommit=%s\nversion=%s\nrun_id=%s\n' \
  "$TAG" "$tag_commit" "$VERSION" "$RUN_ID" > "$OUTPUT_DIR/BUILD-INFO.txt"
find "${OUTPUT_DIRS[@]}" \
  -type f \( -name '*.dmg' -o -name '*.exe' -o -name '*.AppImage' \) \
  -exec shasum -a 256 {} + > "$OUTPUT_DIR/SHA256SUMS.txt"

echo "Selected platform builds succeeded."
echo "Release assets and checksums: $OUTPUT_DIR"
