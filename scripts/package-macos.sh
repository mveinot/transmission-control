#!/usr/bin/env bash
set -euo pipefail

ARCH="$(uname -m)"
APP_NAME="Planetary"
QT_DIR="$HOME/Qt/6.11.1/macos"
BUILD_DIR="build-release"
APP_PATH="$BUILD_DIR/$APP_NAME.app"
DMG_ROOT="$BUILD_DIR/dmg-root"
VERSION_FILE="${BUILD_DIR}/planetary-version.txt"

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

codesign --force --deep --options runtime --timestamp --sign "Developer ID Application: Mark Veinot (TYR38WGV73)" "$APP_PATH"

rm -rf "$DMG_ROOT"
mkdir -p "$DMG_ROOT"
cp -R "$APP_PATH" "$DMG_ROOT/"
ln -s /Applications "$DMG_ROOT/Applications"

DMG_PATH="$BUILD_DIR/${APP_NAME}-${VERSION}-macOS-${ARCH}.dmg"

hdiutil create \
  -volname "$APP_NAME $VERSION" \
  -srcfolder "$DMG_ROOT" \
  -ov \
  -format UDZO \
  "$DMG_PATH"

echo "Created: $DMG_PATH"
