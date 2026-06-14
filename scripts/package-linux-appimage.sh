#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Planetary"
EXECUTABLE_NAME="Planetary"
VERSION_FILE="version.h"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-appimage"
APPDIR="${ROOT_DIR}/${APP_NAME}.AppDir"
DIST_DIR="${ROOT_DIR}/dist"

ARCH="$(uname -m)"
case "$ARCH" in
  x86_64)
    APPIMAGE_ARCH="x86_64"
    ;;
  aarch64|arm64)
    APPIMAGE_ARCH="aarch64"
    ;;
  *)
    APPIMAGE_ARCH="$ARCH"
    ;;
esac

VERSION="$(grep -E '^#define[[:space:]]+__PLANETARY_VERSION__' "${ROOT_DIR}/${VERSION_FILE}" \
  | sed -E 's/.*"([^"]+)".*/\1/')"

if [[ -z "$VERSION" ]]; then
  echo "Could not extract __PLANETARY_VERSION__ from ${VERSION_FILE}"
  exit 1
fi

echo "Packaging ${APP_NAME} ${VERSION} for Linux ${APPIMAGE_ARCH}"

mkdir -p "$DIST_DIR"

rm -rf "$BUILD_DIR" "$APPDIR"
mkdir -p "$BUILD_DIR" "$APPDIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr

cmake --build "$BUILD_DIR" --config Release -j"$(nproc)"

DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"

mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
mkdir -p "${APPDIR}/usr/share/planetary/geoip"
mkdir -p "${APPDIR}/usr/share/planetary/licenses"

if [[ -f "${ROOT_DIR}/packaging/linux/planetary.desktop" ]]; then
  cp "${ROOT_DIR}/packaging/linux/planetary.desktop" \
     "${APPDIR}/usr/share/applications/planetary.desktop"
  cp "${ROOT_DIR}/packaging/linux/planetary.desktop" \
     "${APPDIR}/planetary.desktop"
else
  cat > "${APPDIR}/planetary.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Planetary
Comment=Qt Transmission remote client
Exec=${EXECUTABLE_NAME}
Icon=planetary
Categories=Network;FileTransfer;P2P;Qt;
Terminal=false
StartupNotify=true
EOF
  cp "${APPDIR}/planetary.desktop" \
     "${APPDIR}/usr/share/applications/planetary.desktop"
fi

#if [[ -f "${ROOT_DIR}/Resources/icons/planetary.png" ]]; then
  #cp "${ROOT_DIR}/Resources/icons/planetary.png" \
     #"${APPDIR}/usr/share/icons/hicolor/256x256/apps/planetary.png"
  #cp "${ROOT_DIR}/Resources/icons/planetary.png" \
     #"${APPDIR}/planetary.png"
#fi

ICON_SOURCE="${ROOT_DIR}/Resources/icons/planetary.png"
ICON_APPDIR="${APPDIR}/planetary.png"
ICON_HICOLOR="${APPDIR}/usr/share/icons/hicolor/512x512/apps/planetary.png"

mkdir -p "${APPDIR}/usr/share/icons/hicolor/512x512/apps"

if [[ -f "$ICON_SOURCE" ]]; then
  if command -v magick >/dev/null 2>&1; then
    magick "$ICON_SOURCE" \
      -resize 512x512 \
      -background none \
      -gravity center \
      -extent 512x512 \
      "$ICON_APPDIR"

    cp "$ICON_APPDIR" "$ICON_HICOLOR"

  elif command -v convert >/dev/null 2>&1; then
    convert "$ICON_SOURCE" \
      -resize 512x512 \
      -background none \
      -gravity center \
      -extent 512x512 \
      "$ICON_APPDIR"

    cp "$ICON_APPDIR" "$ICON_HICOLOR"

  else
    echo "ImageMagick is required to normalize the AppImage icon."
    echo "Install it with: sudo apt install imagemagick"
    exit 1
  fi
else
  echo "Missing icon source: $ICON_SOURCE"
  exit 1
fi

if [[ -f "${ROOT_DIR}/Resources/geoip/dbip-country-lite-2026-06.mmdb" ]]; then
  cp "${ROOT_DIR}/Resources/geoip/dbip-country-lite-2026-06.mmdb" \
     "${APPDIR}/usr/share/planetary/geoip/"
fi

if [[ -f "${ROOT_DIR}/LICENSE" ]]; then
  cp "${ROOT_DIR}/LICENSE" \
     "${APPDIR}/usr/share/planetary/licenses/"
fi

if [[ -f "${ROOT_DIR}/THIRD_PARTY_NOTICES.txt" ]]; then
  cp "${ROOT_DIR}/THIRD_PARTY_NOTICES.txt" \
     "${APPDIR}/usr/share/planetary/licenses/"
fi

if [[ -f "${ROOT_DIR}/packaging/linux/AppRun" ]]; then
  cp "${ROOT_DIR}/packaging/linux/AppRun" "${APPDIR}/AppRun"
  chmod +x "${APPDIR}/AppRun"
fi

LINUXDEPLOY="${ROOT_DIR}/tools/linuxdeploy-${APPIMAGE_ARCH}.AppImage"
QT_PLUGIN="${ROOT_DIR}/tools/linuxdeploy-plugin-qt-${APPIMAGE_ARCH}.AppImage"

if [[ ! -x "$LINUXDEPLOY" ]]; then
  echo "Missing or not executable: $LINUXDEPLOY"
  echo "Download linuxdeploy for ${APPIMAGE_ARCH} into tools/"
  exit 1
fi

if [[ ! -x "$QT_PLUGIN" ]]; then
  echo "Missing or not executable: $QT_PLUGIN"
  echo "Download linuxdeploy-plugin-qt for ${APPIMAGE_ARCH} into tools/"
  exit 1
fi

export VERSION="$VERSION"
export ARCH="$APPIMAGE_ARCH"

"$LINUXDEPLOY" \
  --appdir "$APPDIR" \
  --executable "${APPDIR}/usr/bin/${EXECUTABLE_NAME}" \
  --desktop-file "${APPDIR}/planetary.desktop" \
  --icon-file "${APPDIR}/planetary.png" \
  --plugin qt \
  --output appimage

OUTPUT_APPIMAGE="${APP_NAME}-${VERSION}-linux-${APPIMAGE_ARCH}.AppImage"

FOUND_APPIMAGE="$(find "$ROOT_DIR" -maxdepth 1 -type f -name "*.AppImage" | head -n 1 || true)"

if [[ -z "$FOUND_APPIMAGE" ]]; then
  echo "Could not find generated AppImage"
  exit 1
fi

mv "$FOUND_APPIMAGE" "${DIST_DIR}/${OUTPUT_APPIMAGE}"
chmod +x "${DIST_DIR}/${OUTPUT_APPIMAGE}"

echo "Created: ${DIST_DIR}/${OUTPUT_APPIMAGE}"
