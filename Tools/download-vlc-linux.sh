#!/bin/bash
# Download VLC 3.x shared libraries from Ubuntu 22.04 (jammy) packages
# and extract them into Libs/vlc/linux/{x86_64|aarch64}/.
#
# Usage:
#   ./Tools/download-vlc-linux.sh [x86_64|aarch64]
#
# Defaults to x86_64 if no argument is given.
# Requires: wget, ar, tar, zstd (sudo apt install wget zstd).

set -euo pipefail

# Check required tools.
for tool in wget ar zstd; do
    if ! command -v "$tool" &>/dev/null; then
        echo "Error: '$tool' is required but not found. Install it with: sudo apt install $tool"
        exit 1
    fi
done

ARCH="${1:-x86_64}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/Libs/vlc/linux/$ARCH"
WORK_DIR="$(mktemp -d)"

trap 'rm -rf "$WORK_DIR"' EXIT

# Map architecture to Debian package architecture and Ubuntu mirror path.
case "$ARCH" in
    x86_64)
        DEB_ARCH="amd64"
        MIRROR="http://archive.ubuntu.com/ubuntu"
        ;;
    aarch64)
        DEB_ARCH="arm64"
        MIRROR="http://ports.ubuntu.com/ubuntu-ports"
        ;;
    *)
        echo "Error: Unsupported architecture '$ARCH'. Use x86_64 or aarch64."
        exit 1
        ;;
esac

DIST="jammy"  # Ubuntu 22.04 LTS

# Package list: VLC core + base plugins + FFmpeg dependencies.
PACKAGES=(
    "libvlc5"
    "libvlccore9"
    "vlc-plugin-base"
    "libavcodec58"
    "libavformat58"
    "libswscale5"
    "libavutil56"
    "libswresample3"
)

echo "==> Downloading VLC packages for $ARCH ($DEB_ARCH) from Ubuntu $DIST..."
mkdir -p "$OUTPUT_DIR/plugins"

# Fetch the Packages index to find exact URLs.
PACKAGES_URL="$MIRROR/dists/$DIST/universe/binary-$DEB_ARCH/Packages.gz"
PACKAGES_MAIN_URL="$MIRROR/dists/$DIST/main/binary-$DEB_ARCH/Packages.gz"

echo "    Fetching package indices..."
wget -q -O "$WORK_DIR/Packages-universe.gz" "$PACKAGES_URL"
wget -q -O "$WORK_DIR/Packages-main.gz" "$PACKAGES_MAIN_URL"
gunzip -f "$WORK_DIR/Packages-universe.gz"
gunzip -f "$WORK_DIR/Packages-main.gz"
cat "$WORK_DIR/Packages-universe" "$WORK_DIR/Packages-main" > "$WORK_DIR/Packages"

download_and_extract() {
    local pkg_name="$1"
    echo "    Processing $pkg_name..."

    # Find the .deb filename from the Packages index.
    local filename
    filename=$(awk -v pkg="$pkg_name" '
        /^Package: / { found = ($2 == pkg) }
        found && /^Filename: / { print $2; exit }
    ' "$WORK_DIR/Packages")

    if [ -z "$filename" ]; then
        echo "    WARNING: Package '$pkg_name' not found in index, skipping."
        return
    fi

    local url="$MIRROR/$filename"
    local deb_file="$WORK_DIR/$(basename "$filename")"

    wget -q -O "$deb_file" "$url"

    # Extract .deb: ar extracts data.tar.*, then tar extracts files.
    local extract_dir="$WORK_DIR/extract-$pkg_name"
    mkdir -p "$extract_dir"
    cd "$extract_dir"
    ar x "$deb_file"

    # data.tar may be .xz, .zst, or .gz.
    local data_tar
    data_tar=$(ls data.tar.* 2>/dev/null | head -1)
    if [ -z "$data_tar" ]; then
        echo "    WARNING: No data.tar found in $pkg_name, skipping."
        cd "$WORK_DIR"
        return
    fi

    # Handle .tar.zst separately since tar may lack built-in zstd support.
    if [[ "$data_tar" == *.zst ]]; then
        zstd -dq "$data_tar" -o data.tar && tar xf data.tar
    else
        tar xf "$data_tar"
    fi
    cd "$WORK_DIR"
}

for pkg in "${PACKAGES[@]}"; do
    download_and_extract "$pkg"
done

echo "==> Collecting shared libraries..."

# Copy VLC core libraries.
find "$WORK_DIR" -name "libvlc.so*" -not -type d | while read -r f; do
    cp "$f" "$OUTPUT_DIR/"
done
find "$WORK_DIR" -name "libvlccore.so*" -not -type d | while read -r f; do
    cp "$f" "$OUTPUT_DIR/"
done

# Copy VLC plugins.
find "$WORK_DIR" -path "*/vlc/plugins/*.so" | while read -r f; do
    # Preserve plugin subdirectory structure.
    rel=$(echo "$f" | sed 's|.*/vlc/plugins/|plugins/|')
    dir=$(dirname "$OUTPUT_DIR/$rel")
    mkdir -p "$dir"
    cp "$f" "$OUTPUT_DIR/$rel"
done

# Copy FFmpeg libraries needed by VLC plugins.
for lib_pattern in "libavcodec.so*" "libavformat.so*" "libswscale.so*" "libavutil.so*" "libswresample.so*"; do
    find "$WORK_DIR" -name "$lib_pattern" -not -type d | while read -r f; do
        cp "$f" "$OUTPUT_DIR/"
    done
done

# Create soname copies for compatibility (e.g., libvlc.so -> libvlc.so.5.6.1).
# Uses copies instead of symlinks for NTFS/WSL compatibility.
cd "$OUTPUT_DIR"
for sofile in libvlc.so.* libvlccore.so.*; do
    [ -f "$sofile" ] || continue
    base="${sofile%%.*}"
    soname=$(echo "$sofile" | grep -oP '^[^.]+\.so\.\d+')
    [ -n "$soname" ] && [ ! -e "$soname" ] && cp "$sofile" "$soname"
    [ ! -e "${base}.so" ] && cp "$sofile" "${base}.so"
done

echo "==> VLC libraries installed to $OUTPUT_DIR"
echo ""
echo "Contents:"
ls -la "$OUTPUT_DIR/"
echo ""
if [ -d "$OUTPUT_DIR/plugins" ]; then
    echo "Plugins:"
    find "$OUTPUT_DIR/plugins" -name "*.so" | wc -l
    echo " plugin .so files"
fi
