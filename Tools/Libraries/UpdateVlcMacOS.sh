#!/usr/bin/env bash
# Updates the bundled libVLC of macOS in Libs/vlc/macos from the universal (arm64 and x86_64) VLC disk image of
# VideoLAN: libvlc, libvlccore and the plugins listed in Libs/vlc/plugins.txt.
#
# Usage: Tools/Libraries/UpdateVlcMacOS.sh [version]
# Requires curl and 7z (p7zip) on Linux, or hdiutil on macOS.

set -euo pipefail

VERSION="${1:-3.0.23}"
ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
VLC_DIR="$ROOT_DIR/Libs/vlc"
WORK_DIR="${TMPDIR:-/tmp}/ten-vlc-macos"
IMAGE="$WORK_DIR/vlc-$VERSION-universal.dmg"

mkdir -p "$WORK_DIR"
[ -f "$IMAGE" ] || curl -sSfL -o "$IMAGE" "https://download.videolan.org/pub/videolan/vlc/$VERSION/macosx/vlc-$VERSION-universal.dmg"

EXTRACT_DIR="$WORK_DIR/extract"
rm -rf "$EXTRACT_DIR"
mkdir -p "$EXTRACT_DIR"
if command -v hdiutil > /dev/null; then
    MOUNT_DIR="$WORK_DIR/mount"
    hdiutil attach -nobrowse -readonly -mountpoint "$MOUNT_DIR" "$IMAGE" > /dev/null
    cp -R "$MOUNT_DIR/VLC.app" "$EXTRACT_DIR/"
    hdiutil detach "$MOUNT_DIR" > /dev/null
else
    7z x -y -bso0 -bsp0 -o"$EXTRACT_DIR" "$IMAGE" > /dev/null || true
fi

APP_DIR="$(find "$EXTRACT_DIR" -type d -name "VLC.app" -print -quit)"
[ -n "$APP_DIR" ] || { echo "Error: VLC.app not found in $IMAGE."; exit 1; }
LIBRARY_DIR="$APP_DIR/Contents/MacOS/lib"
PLUGIN_DIR="$APP_DIR/Contents/MacOS/plugins"

OUTPUT_DIR="$VLC_DIR/macos"
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/plugins"
cp -L "$LIBRARY_DIR/libvlc.dylib" "$LIBRARY_DIR/libvlccore.dylib" "$OUTPUT_DIR/"

# The macOS plugins are in one folder; they are kept in the folders of the other platforms.
while read -r plugin; do
    plugin="${plugin%$'\r'}"
    if [ -z "$plugin" ] || [[ "$plugin" == \#* ]]; then
        continue
    fi

    source="$(find "$PLUGIN_DIR" -name "$(basename "$plugin").dylib" -print -quit)"
    [ -n "$source" ] || { echo "Error: plugin $plugin not found."; exit 1; }
    mkdir -p "$OUTPUT_DIR/plugins/$(dirname "$plugin")"
    cp -L "$source" "$OUTPUT_DIR/plugins/$plugin.dylib"
done < "$VLC_DIR/plugins.txt"

echo "libVLC $VERSION for macOS: $(find "$OUTPUT_DIR/plugins" -name '*.dylib' | wc -l) plugins, $(du -sh "$OUTPUT_DIR" | cut -f1)"
