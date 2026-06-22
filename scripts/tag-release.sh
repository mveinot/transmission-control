#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Planetary"
DEFAULT_BRANCH="main"
VERSION_FILE="PLANETARY_VERSION"
REMOTE_NAME="origin"

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

if [[ ! "$BASE_VERSION" =~ ^[0-9]+.[0-9]+.[0-9]+$ ]]; then
echo "$VERSION_FILE must contain major.minor.patch, e.g. 0.9.0"
echo "Found: $BASE_VERSION"
exit 1
fi

if ! git remote get-url "$REMOTE_NAME" >/dev/null 2>&1; then
echo "Remote '$REMOTE_NAME' does not exist."
exit 1
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"

if [[ "$CURRENT_BRANCH" != "$DEFAULT_BRANCH" ]]; then
echo "You are on branch '$CURRENT_BRANCH', not '$DEFAULT_BRANCH'."
echo "Refusing to tag from the wrong branch."
exit 1
fi

echo "Fetching latest branch and tags from ${REMOTE_NAME}..."
git fetch "$REMOTE_NAME" "$DEFAULT_BRANCH"
git fetch --tags "$REMOTE_NAME"

git pull --ff-only "$REMOTE_NAME" "$DEFAULT_BRANCH"

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

LOCAL_TAG_EXISTS=false
REMOTE_TAG_EXISTS=false

if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
LOCAL_TAG_EXISTS=true
fi

if git ls-remote --exit-code --tags "$REMOTE_NAME" "refs/tags/${TAG}" >/dev/null 2>&1; then
REMOTE_TAG_EXISTS=true
fi

if [[ "$LOCAL_TAG_EXISTS" == true ]]; then
LOCAL_TAG_COMMIT="$(git rev-list -n 1 "$TAG")"
else
LOCAL_TAG_COMMIT=""
fi

if [[ "$REMOTE_TAG_EXISTS" == true ]]; then
REMOTE_TAG_COMMIT="$(git ls-remote --tags "$REMOTE_NAME" "refs/tags/${TAG}" | awk '{print $1}')"
else
REMOTE_TAG_COMMIT=""
fi

if [[ "$LOCAL_TAG_EXISTS" == true && "$LOCAL_TAG_COMMIT" != "$(git rev-parse HEAD)" ]]; then
echo "Local tag $TAG exists, but it does not point at HEAD."
echo "Local tag commit: $LOCAL_TAG_COMMIT"
echo "Current HEAD:      $(git rev-parse HEAD)"
echo "Refusing to move an existing tag."
exit 1
fi

if [[ "$LOCAL_TAG_EXISTS" == true && "$REMOTE_TAG_EXISTS" == true ]]; then
echo "Tag already exists locally and on ${REMOTE_NAME}: $TAG"
echo "Nothing to do."
exit 0
fi

if [[ "$LOCAL_TAG_EXISTS" == true && "$REMOTE_TAG_EXISTS" == false ]]; then
echo "Tag exists locally but not on ${REMOTE_NAME}. Pushing: $TAG"
git push "$REMOTE_NAME" "$TAG"
echo "Pushed existing local tag: $TAG"
exit 0
fi

if [[ "$LOCAL_TAG_EXISTS" == false && "$REMOTE_TAG_EXISTS" == true ]]; then
echo "Tag exists on ${REMOTE_NAME} but not locally: $TAG"
echo "Fetching tags again..."
git fetch --tags "$REMOTE_NAME"

if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
echo "Fetched existing remote tag: $TAG"
exit 0
fi

echo "Remote tag exists but could not be fetched cleanly."
exit 1
fi

git tag -a "$TAG" 
-m "${APP_NAME} ${TAG}" 
-m "Commit: ${COMMIT}"

git push "$REMOTE_NAME" "$TAG"

echo
echo "Created and pushed release tag:"
echo "$TAG"

