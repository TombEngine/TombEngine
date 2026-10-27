#!/usr/bin/env bash
# Builds the FFmpeg libraries used by the libVLC avcodec and swscale plugins of Linux: FFmpeg 5.1, the release the
# Debian 12 plugins are built against, with only the decoders and parsers of common video files and no external
# libraries. The Windows and macOS plugins of VideoLAN embed a similar build of FFmpeg.
#
# Usage: Tools/Libraries/BuildFfmpegLinux.sh <x86_64|aarch64> <output directory>
# Requires make and a C compiler for the architecture: gcc and nasm for x86_64, aarch64-linux-gnu-gcc when building
# aarch64 on another architecture.

set -euo pipefail

FFMPEG_VERSION="5.1.10"
ARCH="$1"
OUTPUT_DIR="$2"
WORK_DIR="${TMPDIR:-/tmp}/ten-ffmpeg-linux"

DECODERS="h264,hevc,mpeg4,mpeg1video,mpeg2video,msmpeg4v1,msmpeg4v2,msmpeg4v3,h263,h263p,vp8,vp9,vp3,theora,mjpeg,\
cinepak,msvideo1,wmv1,wmv2,wmv3,vc1,indeo3,indeo4,indeo5,aac,aac_latm,mp1,mp2,mp3,mp1float,mp2float,mp3float,ac3,eac3,\
vorbis,opus,flac,alac,wmav1,wmav2,pcm_s16le,pcm_s16be,pcm_s24le,pcm_u8,pcm_f32le,pcm_alaw,pcm_mulaw,adpcm_ima_wav,\
adpcm_ima_qt,adpcm_ms"
PARSERS="h264,hevc,mpeg4video,mpegvideo,h263,vp8,vp9,vp3,mjpeg,vc1,aac,aac_latm,mpegaudio,ac3,vorbis,opus,flac"

CONFIGURE_ARGS=(
    --prefix="$WORK_DIR/install-$ARCH"
    --enable-shared --disable-static --enable-pic
    --disable-programs --disable-doc --disable-debug
    --disable-avdevice --disable-avformat --disable-avfilter --disable-postproc --disable-swresample --disable-network
    --disable-autodetect --disable-everything
    --enable-decoder="$DECODERS" --enable-parser="$PARSERS"
)

case "$ARCH" in
    x86_64)
        if ! command -v nasm > /dev/null; then
            echo "Error: nasm is required for the x86_64 assembly decoders."
            exit 1
        fi
        ;;
    aarch64)
        if [ "$(uname -m)" != "aarch64" ]; then
            command -v aarch64-linux-gnu-gcc > /dev/null || { echo "Error: aarch64-linux-gnu-gcc is required."; exit 1; }
            CONFIGURE_ARGS+=(--enable-cross-compile --arch=aarch64 --target-os=linux --cross-prefix=aarch64-linux-gnu-)
        fi

        # glibc 2.38 added a new fmod version on aarch64; binding the original one keeps the glibc 2.35 requirement of
        # the Debian plugins. The flags also reach the assembly sources, which must skip the C statement.
        mkdir -p "$WORK_DIR"
        printf '#ifndef __ASSEMBLER__\n__asm__(".symver fmod, fmod@GLIBC_2.17");\n#endif\n' > "$WORK_DIR/glibc-compat.h"
        CONFIGURE_ARGS+=(--extra-cflags="-include $WORK_DIR/glibc-compat.h")
        ;;
    *)
        echo "Error: unsupported architecture $ARCH."
        exit 1
        ;;
esac

mkdir -p "$WORK_DIR"
SOURCE_PACKAGE="$WORK_DIR/ffmpeg-$FFMPEG_VERSION.tar.xz"
[ -f "$SOURCE_PACKAGE" ] || curl -sSfL -o "$SOURCE_PACKAGE" "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz"

BUILD_DIR="$WORK_DIR/build-$ARCH"
rm -rf "$BUILD_DIR" "$WORK_DIR/install-$ARCH"
mkdir -p "$BUILD_DIR"
tar -xJf "$SOURCE_PACKAGE" -C "$BUILD_DIR" --strip-components=1

echo "    Building FFmpeg $FFMPEG_VERSION for $ARCH"
if ! (cd "$BUILD_DIR" && ./configure "${CONFIGURE_ARGS[@]}" > configure.log 2>&1 && make -j"$(nproc)" > make.log 2>&1 && make install > /dev/null); then
    echo "Error: FFmpeg build failed; end of the logs in $BUILD_DIR:"
    tail -n 20 "$BUILD_DIR/configure.log" "$BUILD_DIR/ffbuild/config.log" "$BUILD_DIR/make.log" 2> /dev/null
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
for library in libavcodec.so.59 libavutil.so.57 libswscale.so.6; do
    cp -L "$WORK_DIR/install-$ARCH/lib/$library" "$OUTPUT_DIR/$library"
done
