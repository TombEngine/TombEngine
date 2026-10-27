#!/usr/bin/env bash
# Packs the Linux engine build into one AppImage: the executable with the bundled BASS, libVLC and libVLC plugins that
# the build copies next to it. The game data stays outside: the AppImage is placed in the game folder, next to Scripts,
# Shaders and the levels, and runs with that folder as game directory.
#
# Usage: Tools/Packaging/BuildAppImage.sh [Release|Debug]
# Requires curl; appimagetool is downloaded on first use.

set -euo pipefail

CONFIGURATION="${1:-Release}"
ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BIN_DIR="$ROOT_DIR/Build/$CONFIGURATION/Bin/x64"
ARCH="$(uname -m)"
WORK_DIR="${TMPDIR:-/tmp}/ten-appimage"
APP_DIR="$WORK_DIR/TombEngine.AppDir"
OUTPUT="$ROOT_DIR/Build/$CONFIGURATION/TombEngine-$ARCH.AppImage"

[ -x "$BIN_DIR/TombEngine" ] || { echo "Error: build the $CONFIGURATION configuration first ($BIN_DIR/TombEngine)."; exit 1; }

APPIMAGETOOL="$WORK_DIR/appimagetool-$ARCH.AppImage"
mkdir -p "$WORK_DIR"
if [ ! -x "$APPIMAGETOOL" ]; then
    curl -sSfL -o "$APPIMAGETOOL" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$ARCH.AppImage"
    chmod +x "$APPIMAGETOOL"
fi

rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/usr/bin"
cp -a "$BIN_DIR/." "$APP_DIR/usr/bin/"
rm -f "$APP_DIR/usr/bin/plugins/plugins.dat"
cp "$ROOT_DIR/TEN logo.png" "$APP_DIR/TombEngine.png"

cat > "$APP_DIR/TombEngine.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=TombEngine
Exec=TombEngine
Icon=TombEngine
Categories=Game;
Terminal=false
EOF

# Without -gamedir the game folder is the one containing the AppImage, as the folder of the executable on Windows.
cat > "$APP_DIR/AppRun" << 'EOF'
#!/bin/sh
ENGINE_DIR="$(dirname "$(readlink -f "$0")")/usr/bin"
for arg in "$@"; do
    case "$arg" in
        -gamedir|/gamedir|gamedir) exec "$ENGINE_DIR/TombEngine" "$@" ;;
    esac
done
exec "$ENGINE_DIR/TombEngine" -gamedir "$(dirname "${APPIMAGE:-$0}")" "$@"
EOF
chmod +x "$APP_DIR/AppRun"

# The AppImage runtime is read-only, so the libVLC plugin cache is rebuilt in memory on each start; the plugin set is
# small enough for this to be fast.
ARCH="$ARCH" "$APPIMAGETOOL" --appimage-extract-and-run "$APP_DIR" "$OUTPUT" > /dev/null
echo "AppImage: $OUTPUT"
