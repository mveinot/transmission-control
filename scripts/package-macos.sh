#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

APP_NAME="Planetary"
QT_DIR="${QT_DIR:-$HOME/Qt/6.11.1/macos}"
MAXMINDDB_ROOT="${MAXMINDDB_ROOT:-$HOME/Developer/Dependencies/libmaxminddb-1.13.3/install-universal-macos13}"
MACOS_ARCHITECTURES="x86_64;arm64"
MACOS_DEPLOYMENT_TARGET="13.0"
BUILD_DIR="${BUILD_DIR:-build-macos-universal-release}"
APP_PATH="$BUILD_DIR/$APP_NAME.app"
APP_EXECUTABLE="$APP_PATH/Contents/MacOS/$APP_NAME"
DMG_ROOT="$BUILD_DIR/dmg-root"
VERSION_FILE="${BUILD_DIR}/planetary-version.txt"
BUILD_KEYCHAIN="${BUILD_KEYCHAIN:-$HOME/Library/Keychains/planetary-build.keychain-db}"
NOTARY_ARCHIVE="$BUILD_DIR/$APP_NAME-notarization.zip"
SIGNING_IDENTITY="${SIGNING_IDENTITY:-Developer ID Application: Mark Veinot (TYR38WGV73)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-PlanetaryNotary}"

fail() {
  echo "error: $*" >&2
  exit 1
}

for tool in awk cmake codesign ditto file find grep hdiutil install_name_tool lipo nm otool plutil security spctl vtool xattr xcrun; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    fail "Missing required tool: $tool"
  fi
done

if [[ ! -x "$QT_DIR/bin/macdeployqt" ]]; then
  fail "Missing macdeployqt: $QT_DIR/bin/macdeployqt"
fi

if [[ ! -f "$BUILD_KEYCHAIN" ]]; then
  fail "Missing signing keychain: $BUILD_KEYCHAIN"
fi

if ! security find-identity -v -p codesigning "$BUILD_KEYCHAIN" |
  grep -F "$SIGNING_IDENTITY" >/dev/null; then
  fail "Signing identity is not available in $BUILD_KEYCHAIN: $SIGNING_IDENTITY"
fi

if [[ ! -f "$MAXMINDDB_ROOT/lib/libmaxminddb.a" ]]; then
  fail "Missing universal libmaxminddb: $MAXMINDDB_ROOT/lib/libmaxminddb.a"
fi

if ! lipo "$MAXMINDDB_ROOT/lib/libmaxminddb.a" -verify_arch x86_64 arm64; then
  fail "libmaxminddb is not universal: $MAXMINDDB_ROOT/lib/libmaxminddb.a"
fi

for dependency_arch in x86_64 arm64; do
  archive_minos_count=0
  while IFS= read -r archive_minos; do
    archive_minos_count=$((archive_minos_count + 1))
    if [[ "$archive_minos" != "$MACOS_DEPLOYMENT_TARGET" ]]; then
      fail "libmaxminddb ($dependency_arch) targets macOS $archive_minos, expected $MACOS_DEPLOYMENT_TARGET"
    fi
  done < <(
    otool -arch "$dependency_arch" -l "$MAXMINDDB_ROOT/lib/libmaxminddb.a" |
      awk '$1 == "minos" { print $2 }'
  )

  if [[ "$archive_minos_count" -eq 0 ]]; then
    fail "No macOS build-version records found in libmaxminddb ($dependency_arch)"
  fi
done

verify_macho() {
  local artifact_path="$1"
  local artifact_minos
  local artifact_rpath
  local minos_count=0

  if ! file -b "$artifact_path" | grep -q 'Mach-O'; then
    return
  fi

  if ! lipo "$artifact_path" -verify_arch x86_64 arm64; then
    fail "Mach-O is not universal: $artifact_path"
  fi

  while IFS= read -r artifact_minos; do
    minos_count=$((minos_count + 1))
    if [[ "$artifact_minos" != "$MACOS_DEPLOYMENT_TARGET" ]]; then
      fail "$artifact_path targets macOS $artifact_minos, expected $MACOS_DEPLOYMENT_TARGET"
    fi
  done < <(vtool -show-build "$artifact_path" | awk '$1 == "minos" { print $2 }')

  if [[ "$minos_count" -ne 2 ]]; then
    fail "Expected two macOS build-version records in $artifact_path, found $minos_count"
  fi

  if otool -L "$artifact_path" | grep -E '^[[:space:]]+(/usr/local/|/opt/homebrew/|/Users/)' >/dev/null; then
    otool -L "$artifact_path" >&2
    fail "Mach-O contains a non-portable library dependency: $artifact_path"
  fi

  while IFS= read -r artifact_rpath; do
    if [[ "$artifact_rpath" == /* ]]; then
      fail "Mach-O contains an absolute runtime search path ($artifact_rpath): $artifact_path"
    fi
  done < <(otool -l "$artifact_path" |
    awk '/LC_RPATH/ { found = 1; next } found && $1 == "path" { print $2; found = 0 }')
}

list_rpaths() {
  otool -l "$1" |
    awk '/LC_RPATH/ { found = 1; next } found && $1 == "path" { print $2; found = 0 }'
}

prepare_bundle_rpaths() {
  local artifact_path
  local artifact_rpath
  local plugin_rpaths

  # Remove build-machine search paths from every deployed Mach-O.
  while IFS= read -r -d '' artifact_path; do
    if ! file -b "$artifact_path" | grep -q 'Mach-O'; then
      continue
    fi

    while IFS= read -r artifact_rpath; do
      if [[ "$artifact_rpath" == /* ]]; then
        install_name_tool -delete_rpath "$artifact_rpath" "$artifact_path"
      fi
    done < <(list_rpaths "$artifact_path")
  done < <(find "$APP_PATH/Contents" -type f -print0)

  if ! list_rpaths "$APP_EXECUTABLE" |
    grep -Fx '@executable_path/../Frameworks' >/dev/null; then
    install_name_tool -add_rpath '@executable_path/../Frameworks' "$APP_EXECUTABLE"
  fi

  # Qt's SDK plugins use ../../lib. In an application bundle their sibling
  # framework directory is ../../Frameworks instead.
  while IFS= read -r -d '' artifact_path; do
    if ! file -b "$artifact_path" | grep -q 'Mach-O'; then
      continue
    fi

    plugin_rpaths="$(list_rpaths "$artifact_path")"
    if grep -Fx '@loader_path/../../lib' <<<"$plugin_rpaths" >/dev/null; then
      if grep -Fx '@loader_path/../../Frameworks' <<<"$plugin_rpaths" >/dev/null; then
        install_name_tool -delete_rpath '@loader_path/../../lib' "$artifact_path"
      else
        install_name_tool -rpath \
          '@loader_path/../../lib' \
          '@loader_path/../../Frameworks' \
          "$artifact_path"
      fi
    fi
  done < <(find "$APP_PATH/Contents/PlugIns" -type f -print0)
}

verify_app_bundle() {
  local artifact_path
  local executable_count=0
  local plist_target

  while IFS= read -r -d '' artifact_path; do
    if file -b "$artifact_path" | grep -q 'Mach-O'; then
      verify_macho "$artifact_path"
      executable_count=$((executable_count + 1))
    fi
  done < <(find "$APP_PATH/Contents" -type f -print0)

  if [[ "$executable_count" -eq 0 ]]; then
    fail "No Mach-O files found in $APP_PATH"
  fi

  plist_target="$(plutil -extract LSMinimumSystemVersion raw "$APP_PATH/Contents/Info.plist")"
  if [[ "$plist_target" != "$MACOS_DEPLOYMENT_TARGET" ]]; then
    fail "Info.plist targets macOS $plist_target, expected $MACOS_DEPLOYMENT_TARGET"
  fi

  if ! list_rpaths "$APP_EXECUTABLE" |
    grep -Fx '@executable_path/../Frameworks' >/dev/null; then
    fail "Executable is missing its bundled-framework runtime search path"
  fi

  for required_arch in x86_64 arm64; do
    if ! nm -arch "$required_arch" "$APP_EXECUTABLE" | grep ' _MMDB_open$' >/dev/null; then
      fail "Static libmaxminddb symbols are missing from the $required_arch executable"
    fi
  done
}

verify_signing_teams() {
  local artifact_path
  local artifact_team
  local expected_team

  expected_team="$(codesign -dvv "$APP_PATH" 2>&1 |
    awk -F= '$1 == "TeamIdentifier" { print $2 }')"

  if [[ -z "$expected_team" || "$expected_team" == "not set" ]]; then
    fail "Signed app does not have a Team ID"
  fi

  while IFS= read -r -d '' artifact_path; do
    if ! file -b "$artifact_path" | grep -q 'Mach-O'; then
      continue
    fi

    artifact_team="$(codesign -dvv "$artifact_path" 2>&1 |
      awk -F= '$1 == "TeamIdentifier" { print $2 }')"
    if [[ "$artifact_team" != "$expected_team" ]]; then
      fail "$artifact_path is signed by team ${artifact_team:-<none>}, expected $expected_team"
    fi
  done < <(find "$APP_PATH/Contents" -type f -print0)
}

sign_code() {
  codesign \
    --force \
    --options runtime \
    --timestamp \
    --keychain "$BUILD_KEYCHAIN" \
    --sign "$SIGNING_IDENTITY" \
    "$1"
}

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_DIR" \
  -DCMAKE_OSX_ARCHITECTURES="$MACOS_ARCHITECTURES" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_DEPLOYMENT_TARGET" \
  -DPLANETARY_MAXMINDDB_ROOT="$MAXMINDDB_ROOT" \
  -DMAXMINDDB_LIBRARY="$MAXMINDDB_ROOT/lib/libmaxminddb.a"

cmake --build "$BUILD_DIR" --config Release --parallel

if [[ ! -f "$VERSION_FILE" ]]; then
  fail "Missing version file: $VERSION_FILE; CMake configure may have failed"
fi

VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"

if [[ -z "$VERSION" ]]; then
  fail "Version file is empty: $VERSION_FILE"
fi

"$QT_DIR/bin/macdeployqt" "$APP_PATH" -verbose=2
prepare_bundle_rpaths
verify_app_bundle

# Remove resource-fork metadata that can make an otherwise valid bundle fail
# strict signing or notarization after files have been copied into it.
xattr -cr "$APP_PATH"

# Sign nested code from the inside out. The final app signature seals the
# already-signed frameworks and plugins into the bundle.
while IFS= read -r -d '' plugin_path; do
  sign_code "$plugin_path"
done < <(find "$APP_PATH/Contents/PlugIns" -type f \( -name '*.dylib' -o -perm -111 \) -print0)

while IFS= read -r -d '' framework_path; do
  sign_code "$framework_path"
done < <(find "$APP_PATH/Contents/Frameworks" -type d -name '*.framework' -prune -print0)

sign_code "$APP_PATH"

codesign --verify --deep --strict --verbose=2 "$APP_PATH"
verify_signing_teams

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
ditto "$APP_PATH" "$DMG_ROOT/$APP_NAME.app"
ln -s /Applications "$DMG_ROOT/Applications"

DMG_PATH="$BUILD_DIR/${APP_NAME}-${VERSION}-macOS-universal.dmg"
rm -f "$DMG_PATH"

hdiutil create \
  -volname "$APP_NAME $VERSION" \
  -srcfolder "$DMG_ROOT" \
  -ov \
  -format UDZO \
  "$DMG_PATH"

# Sign and notarize the distribution container as well as its enclosed app so
# Gatekeeper can validate either layer without contacting Apple first.
codesign \
  --force \
  --timestamp \
  --keychain "$BUILD_KEYCHAIN" \
  --sign "$SIGNING_IDENTITY" \
  "$DMG_PATH"
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
