#!/usr/bin/env bash
set -euo pipefail

APP_NAME="Planetary"
APP_ID="planetary"
PREFIX="$HOME/.local"

ROOT_DIR="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT_DIR"

BINARY="build-release/${APP_NAME}"
ICON_SOURCE="Resources/icons/planetary-512px.png"
DESKTOP_SOURCE="packaging/linux/planetary.desktop"

if [[ ! -x "$BINARY" ]]; then
  echo "Missing binary: $BINARY"
  echo "Run ./scripts/build-release-linux.sh first."
  exit 1
fi

mkdir -p "$PREFIX/bin"
mkdir -p "$PREFIX/share/applications"
mkdir -p "$PREFIX/share/icons/hicolor/512x512/apps"

cp "$BINARY" "$PREFIX/bin/$APP_NAME"
chmod +x "$PREFIX/bin/$APP_NAME"

if command -v magick >/dev/null 2>&1; then
  magick "$ICON_SOURCE" \
    -resize 512x512 \
    -background none \
    -gravity center \
    -extent 512x512 \
    "$PREFIX/share/icons/hicolor/512x512/apps/${APP_ID}.png"
else
  cp "$ICON_SOURCE" "$PREFIX/share/icons/hicolor/512x512/apps/${APP_ID}.png"
fi

sed "s|^Exec=.*|Exec=$PREFIX/bin/$APP_NAME %U|" "$DESKTOP_SOURCE" \
  > "$PREFIX/share/applications/${APP_ID}.desktop"

chmod +x "$PREFIX/share/applications/${APP_ID}.desktop"

update-desktop-database "$PREFIX/share/applications" 2>/dev/null || true
gtk-update-icon-cache "$PREFIX/share/icons/hicolor" 2>/dev/null || true

xdg-mime default "${APP_ID}.desktop" application/x-bittorrent
xdg-mime default "${APP_ID}.desktop" x-scheme-handler/magnet

echo "Installed $APP_NAME locally."
echo "Desktop file: $PREFIX/share/applications/${APP_ID}.desktop"
echo "Binary:       $PREFIX/bin/$APP_NAME"
echo "Icon:         $PREFIX/share/icons/hicolor/512x512/apps/${APP_ID}.png"
