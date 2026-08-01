#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

ARCH="$(uname -m)"
APP_NAME="Planetary"
QT_DIR="${QT_DIR:-$HOME/Qt/6.11.1/macos}"
BUILD_DIR="${BUILD_DIR:-build-release}"
APP_PATH="$BUILD_DIR/$APP_NAME.app"
DMG_ROOT="$BUILD_DIR/dmg-root"
VERSION_FILE="${BUILD_DIR}/planetary-version.txt"
BUILD_KEYCHAIN="$HOME/Library/Keychains/planetary-build.keychain-db"
NOTARY_ARCHIVE="$BUILD_DIR/$APP_NAME-notarization.zip"
SIGNING_IDENTITY="${SIGNING_IDENTITY:-Developer ID Application: Mark Veinot (TYR38WGV73)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-PlanetaryNotary}"

for tool in cmake codesign ditto hdiutil xcrun; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Missing required tool: $tool"
    exit 1
  fi
done

if [[ ! -x "$QT_DIR/bin/macdeployqt" ]]; then
  echo "Missing macdeployqt: $QT_DIR/bin/macdeployqt"
  exit 1
fi

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_DIR"

cmake --build "$BUILD_DIR" --config Release

if [[ ! -f "$VERSION_FILE" ]]; then
  echo "Missing version file: $VERSION_FILE"
  echo "CMake configure may have failed, or planetary-version.txt is not being generated."
  exit 1
fi

VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"

if [[ -z "$VERSION" ]]; then
  echo "Version file is empty: $VERSION_FILE"
  exit 1
fi

"$QT_DIR/bin/macdeployqt" "$APP_PATH" -verbose=2

if otool -L "$APP_PATH/Contents/MacOS/$APP_NAME" | grep -q "/usr/local/lib/libmaxminddb.dylib"; then
  mkdir -p "$APP_PATH/Contents/Frameworks"
  cp /usr/local/lib/libmaxminddb.dylib "$APP_PATH/Contents/Frameworks/"

  install_name_tool -change \
    /usr/local/lib/libmaxminddb.dylib \
    @executable_path/../Frameworks/libmaxminddb.dylib \
    "$APP_PATH/Contents/MacOS/$APP_NAME"

  install_name_tool -id \
    @rpath/libmaxminddb.dylib \
    "$APP_PATH/Contents/Frameworks/libmaxminddb.dylib"
fi

# Remove resource-fork metadata that can make an otherwise valid bundle fail
# strict signing or notarization after files have been copied into it.
xattr -cr "$APP_PATH"

codesign \
  --force \
  --deep \
  --options runtime \
  --timestamp \
  --keychain "$BUILD_KEYCHAIN" \
  --sign "$SIGNING_IDENTITY" \
  "$APP_PATH"

codesign --verify --deep --strict --verbose=2 "$APP_PATH"

# notarytool accepts a ZIP container for an application bundle. The archive is
# transport-only; the returned ticket is stapled directly to Planetary.app.
rm -f "$NOTARY_ARCHIVE"
ditto -c -k --keepParent "$APP_PATH" "$NOTARY_ARCHIVE"

echo "Submitting $APP_NAME.app for notarization..."
xcrun notarytool submit "$NOTARY_ARCHIVE" \
  --keychain-profile "$NOTARY_PROFILE" \
  --wait

xcrun stapler staple "$APP_PATH"
xcrun stapler validate "$APP_PATH"
rm -f "$NOTARY_ARCHIVE"

rm -rf "$DMG_ROOT"
mkdir -p "$DMG_ROOT"
cp -R "$APP_PATH" "$DMG_ROOT/"
ln -s /Applications "$DMG_ROOT/Applications"

DMG_PATH="$BUILD_DIR/${APP_NAME}-${VERSION}-macOS-${ARCH}.dmg"
rm -f "$DMG_PATH"

hdiutil create \
  -volname "$APP_NAME $VERSION" \
  -srcfolder "$DMG_ROOT" \
  -ov \
  -format UDZO \
  "$DMG_PATH"

# Sign and notarize the distribution container as well as its enclosed app so
# Gatekeeper can validate either layer without contacting Apple first.
codesign --force --timestamp --sign "$SIGNING_IDENTITY" "$DMG_PATH"
codesign --verify --verbose=2 "$DMG_PATH"

echo "Submitting DMG for notarization..."
xcrun notarytool submit "$DMG_PATH" \
  --keychain-profile "$NOTARY_PROFILE" \
  --wait

xcrun stapler staple "$DMG_PATH"
xcrun stapler validate "$DMG_PATH"

spctl --assess --type execute --verbose=2 "$APP_PATH"
spctl --assess --type open --context context:primary-signature --verbose=2 "$DMG_PATH"

echo "Created signed and notarized package: $DMG_PATH"
