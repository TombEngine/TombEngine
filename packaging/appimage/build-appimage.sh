#!/bin/bash
# Build an AppImage for TombEngine.
#
# Prerequisites:
#   - TombEngine already built with CMake
#   - appimagetool on PATH (https://github.com/AppImage/appimagetool)
#
# Usage:
#   ./packaging/appimage/build-appimage.sh [build-dir]
#
# The build directory defaults to "build".

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${1:-build}"
APPDIR="$REPO_ROOT/$BUILD_DIR/AppDir"

echo "==> Installing to AppDir..."
cmake --install "$REPO_ROOT/$BUILD_DIR" --prefix "$APPDIR"

echo "==> Restructuring for AppImage FHS layout..."
# AppImage expects usr/bin, usr/share structure.
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/256x256/apps"

# Move Engine contents (binary + libs) to usr/bin.
mv "$APPDIR"/Engine/* "$APPDIR/usr/bin/"
rmdir "$APPDIR/Engine"

# Move Shaders and Scripts next to the binary (the engine will find them
# via -gamedir, but include them for convenience).
if [ -d "$APPDIR/Shaders" ]; then
    mv "$APPDIR/Shaders" "$APPDIR/usr/bin/../Shaders"
fi
if [ -d "$APPDIR/Scripts" ]; then
    mv "$APPDIR/Scripts" "$APPDIR/usr/bin/../Scripts"
fi

# Desktop entry and icon.
cp "$REPO_ROOT/packaging/appimage/TombEngine.desktop" "$APPDIR/"
cp "$REPO_ROOT/packaging/appimage/TombEngine.desktop" "$APPDIR/usr/share/applications/"

# Use a placeholder icon if no icon exists.
if [ -f "$REPO_ROOT/packaging/appimage/TombEngine.png" ]; then
    cp "$REPO_ROOT/packaging/appimage/TombEngine.png" "$APPDIR/TombEngine.png"
    cp "$REPO_ROOT/packaging/appimage/TombEngine.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/TombEngine.png"
else
    # Create a minimal 1x1 PNG as placeholder.
    echo "Warning: No TombEngine.png icon found, using placeholder."
    python3 -c "
import struct, zlib
def png(w,h):
    raw=b''
    for y in range(h):
        raw+=b'\x00'+b'\xff\x00\x00\xff'*w
    c=zlib.compress(raw)
    sig=b'\x89PNG\r\n\x1a\n'
    def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
    return sig+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',c)+chunk(b'IEND',b'')
open('$APPDIR/TombEngine.png','wb').write(png(1,1))
" 2>/dev/null || touch "$APPDIR/TombEngine.png"
    cp "$APPDIR/TombEngine.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/TombEngine.png"
fi

# AppRun entry point.
cp "$REPO_ROOT/packaging/appimage/AppRun" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"

echo "==> Building AppImage..."
ARCH="$(uname -m)" appimagetool "$APPDIR" "$REPO_ROOT/$BUILD_DIR/TombEngine-$(uname -m).AppImage"

echo "==> Done: $REPO_ROOT/$BUILD_DIR/TombEngine-$(uname -m).AppImage"
