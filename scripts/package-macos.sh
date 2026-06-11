#!/usr/bin/env bash
set -euo pipefail

ARCH="$(uname -m)"
APP_NAME="Planetary"
QT_DIR="$HOME/Qt/6.11.1/macos"
BUILD_DIR="build-release"
APP_PATH="$BUILD_DIR/$APP_NAME.app"
DMG_ROOT="$BUILD_DIR/dmg-root"
DMG_PATH="$BUILD_DIR/${APP_NAME}-macOS-${ARCH}.dmg"

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_DIR"

cmake --build "$BUILD_DIR" --config Release

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

codesign --force --deep --sign - "$APP_PATH"

rm -rf "$DMG_ROOT"
mkdir -p "$DMG_ROOT"
cp -R "$APP_PATH" "$DMG_ROOT/"
ln -s /Applications "$DMG_ROOT/Applications"

hdiutil create \
  -volname "$APP_NAME" \
  -srcfolder "$DMG_ROOT" \
  -ov \
  -format UDZO \
  "$DMG_PATH"

echo "Created: $DMG_PATH"
