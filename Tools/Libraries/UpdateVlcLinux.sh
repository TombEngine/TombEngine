#!/usr/bin/env bash
# Updates the bundled libVLC of Linux in Libs/vlc/linux/<arch> from the Debian 12 packages, which have the same libVLC
# release as the Windows and macOS packages of VideoLAN. Only the plugins listed in Libs/vlc/plugins.txt are kept, with
# the libraries they need; libraries every desktop system provides (glibc, the C++ runtime, X11, GL, audio servers) are
# left to the system, as the AppImage exclude list does.
#
# Usage: Tools/Libraries/UpdateVlcLinux.sh [x86_64] [aarch64]
# Requires curl, ar, tar with xz support and readelf (binutils), and what BuildFfmpegLinux.sh needs.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
VLC_DIR="$ROOT_DIR/Libs/vlc"
WORK_DIR="${TMPDIR:-/tmp}/ten-vlc-linux"
MIRRORS=("http://security.debian.org/debian-security bookworm-security" "http://deb.debian.org/debian bookworm")
ROOT_PACKAGES=(libvlc5 libvlccore9 vlc-plugin-base)
SYSTEM_LIBRARIES='^(ld-linux.*|libc|libm|libmvec|libdl|libpthread|librt|libresolv|libutil|libanl|libstdc\+\+|libgcc_s|libz|libX[A-Za-z0-9]*|libxcb.*|libGL.*|libEGL|libOpenGL|libgbm|libdrm|libwayland-.*|libasound|libpulse.*|libjack|libdbus-1|libsystemd|libudev|libfontconfig|libfreetype|libharfbuzz|libexpat|libuuid|libselinux|libgpg-error|libgcrypt|libcap)\.so'

ARCHS=("$@")
if [ ${#ARCHS[@]} -eq 0 ]; then
    ARCHS=(x86_64 aarch64)
fi

for tool in curl ar tar readelf; do
    command -v "$tool" > /dev/null || { echo "Error: $tool is required."; exit 1; }
done

# Package, file name and first alternative of every dependency, from the security index first, so the newest release wins.
load_index()
{
    local debArch="$1" index="$WORK_DIR/$1/index.tsv"
    [ -f "$index" ] && return
    : > "$index"
    for mirror in "${MIRRORS[@]}"; do
        set -- $mirror
        curl -sSfL "$1/dists/$2/main/binary-$debArch/Packages.xz" | xz -dc | awk -v base="$1" '
            /^Package: / { name = $2; file = ""; deps = "" }
            /^Filename: / { file = base "/" $2 }
            /^(Pre-)?Depends: / { sub(/^(Pre-)?Depends: /, ""); deps = deps (deps == "" ? "" : ", ") $0 }
            /^$/ { if (name != "" && file != "") print name "\t" file "\t" deps; name = "" }' >> "$index"
    done
}

package_field()
{
    awk -F'\t' -v name="$2" -v field="$3" '$1 == name { print $field; exit }' "$WORK_DIR/$1/index.tsv"
}

fetch_package()
{
    local debArch="$1" name="$2" dir="$WORK_DIR/$1/packages/$2"
    [ -d "$dir" ] && return
    local url
    url="$(package_field "$debArch" "$name" 2)"
    [ -n "$url" ] || { echo "Error: package $name not found for $debArch."; exit 1; }

    echo "    $name"
    mkdir -p "$dir.tmp"
    curl -sSfL -o "$dir.tmp/package.deb" "$url"
    (cd "$dir.tmp" && ar x package.deb && tar -xf data.tar.* && rm -f package.deb data.tar.* control.tar.* debian-binary)
    mv "$dir.tmp" "$dir"
}

# The package containing a file, from the folder it was extracted to.
owner_of()
{
    local path="${1#"$WORK_DIR/$2/packages/"}"
    echo "${path%%/*}"
}

find_library()
{
    find "$WORK_DIR/$1/packages" -name "$2" \( -type f -o -type l \) -print -quit
}

# Fetches the dependencies of a package, which provide the libraries its files need.
fetch_dependencies()
{
    local debArch="$1" name="$2" dependency
    IFS=',' read -ra dependencies <<< "$(package_field "$debArch" "$name" 3)"
    for dependency in "${dependencies[@]}"; do
        dependency="${dependency%%|*}"
        dependency="$(echo "$dependency" | sed -e 's/([^)]*)//g' -e 's/:any//' | xargs)"
        if [ -n "$dependency" ] && [ -n "$(package_field "$debArch" "$dependency" 2)" ]; then
            fetch_package "$debArch" "$dependency"
        fi
    done
}

update_arch()
{
    local arch="$1" debArch triplet
    case "$arch" in
        x86_64) debArch=amd64; triplet=x86_64-linux-gnu ;;
        aarch64) debArch=arm64; triplet=aarch64-linux-gnu ;;
        *) echo "Error: unsupported architecture $arch."; exit 1 ;;
    esac

    echo "==> libVLC for $arch"
    mkdir -p "$WORK_DIR/$debArch"
    load_index "$debArch"
    for name in "${ROOT_PACKAGES[@]}"; do
        fetch_package "$debArch" "$name"
    done

    local outDir="$VLC_DIR/linux/$arch" packageDir="$WORK_DIR/$debArch/packages"
    rm -rf "$outDir"
    mkdir -p "$outDir/plugins"

    # Files to bundle, followed by the libraries they need until none is missing.
    local queue=("$packageDir/libvlc5/usr/lib/$triplet/libvlc.so.5")
    cp -L "${queue[0]}" "$outDir/libvlc.so.5"
    local plugin
    while read -r plugin; do
        plugin="${plugin%$'\r'}"
        if [ -z "$plugin" ] || [[ "$plugin" == \#* ]]; then
            continue
        fi
        local source="$packageDir/vlc-plugin-base/usr/lib/$triplet/vlc/plugins/$plugin.so"
        [ -f "$source" ] || source="$(find "$packageDir" -path "*/vlc/plugins/$plugin.so" -print -quit)"
        [ -n "$source" ] || { echo "Error: plugin $plugin not found."; exit 1; }
        mkdir -p "$outDir/plugins/$(dirname "$plugin")"
        cp -L "$source" "$outDir/plugins/$plugin.so"
        queue+=("$source")
    done < "$VLC_DIR/plugins.txt"

    # The Debian FFmpeg libraries need dozens of encoder and graphics libraries; a decoder-only build of the same release
    # replaces them.
    declare -A bundled=()
    local ffmpegDir="$WORK_DIR/$debArch/ffmpeg" library
    bash "$ROOT_DIR/Tools/Libraries/BuildFfmpegLinux.sh" "$arch" "$ffmpegDir"
    for library in "$ffmpegDir"/*.so.*; do
        bundled[$(basename "$library")]=1
        cp "$library" "$outDir/"
        queue+=("$library")
    done

    local file needed path
    while [ ${#queue[@]} -gt 0 ]; do
        file="${queue[0]}"
        queue=("${queue[@]:1}")
        for needed in $(readelf -d "$file" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
            [[ "$needed" =~ $SYSTEM_LIBRARIES ]] && continue
            [ -n "${bundled[$needed]:-}" ] && continue

            path="$(find_library "$debArch" "$needed")"
            if [ -z "$path" ]; then
                fetch_dependencies "$debArch" "$(owner_of "$file" "$debArch")"
                path="$(find_library "$debArch" "$needed")"
            fi
            [ -n "$path" ] || { echo "Error: $needed needed by $(basename "$file") not found."; exit 1; }

            bundled[$needed]=1
            cp -L "$path" "$outDir/$needed"
            queue+=("$path")
        done
    done

    local version
    version="$(package_field "$debArch" libvlc5 2 | sed 's/.*_\([0-9.]*\)-.*/\1/')"
    echo "    libVLC $version: $(ls "$outDir" | grep -c '\.so') libraries, $(find "$outDir/plugins" -name '*.so' | wc -l) plugins, $(du -sh "$outDir" | cut -f1)"
}

for arch in "${ARCHS[@]}"; do
    update_arch "$arch"
done
