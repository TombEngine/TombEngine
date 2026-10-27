#!/usr/bin/env bash
# Checks the bundled libVLC of Linux for this machine's architecture: plays a video with the same setup as the engine
# (libraries found through the RPATH of the executable, plugins through VLC_PLUGIN_PATH, video and audio callbacks) and
# reports the decoded video frames and audio samples.
#
# Usage: Tools/Libraries/TestVlcLinux.sh <video file>
# Requires gcc.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
VIDEO="$(readlink -f "$1")"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

cp -a "$ROOT_DIR/Libs/vlc/linux/$(uname -m)/." "$WORK_DIR/"

cat > "$WORK_DIR/test.c" << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vlc/vlc.h>

static unsigned char frame[4096 * 4096 * 4];
static int frameCount = 0;
static long sampleCount = 0;

static unsigned setup(void** data, char* chroma, unsigned* width, unsigned* height, unsigned* pitches, unsigned* lines)
{
    chroma[0] = 'B'; chroma[1] = 'G'; chroma[2] = 'R'; chroma[3] = 'A';
    *pitches = *width * 4;
    *lines = *height + 128;
    printf("video %ux%u\n", *width, *height);
    return 1;
}

static void* lockFrame(void* data, void** planes) { planes[0] = frame; return NULL; }
static void unlockFrame(void* data, void* picture, void* const* planes) { frameCount++; }
static void playAudio(void* data, const void* samples, unsigned count, int64_t pts) { sampleCount += count; }

int main(int argc, char** argv)
{
    const char* args[] = { "--vout=vdummy", "--aout=adummy", "--no-video-title", "--quiet" };
    libvlc_instance_t* instance = libvlc_new(4, args);
    if (instance == NULL)
    {
        printf("libvlc_new failed\n");
        return 1;
    }

    printf("libVLC %s\n", libvlc_get_version());
    libvlc_media_t* media = libvlc_media_new_path(instance, argv[1]);
    libvlc_media_player_t* player = libvlc_media_player_new_from_media(media);
    libvlc_media_release(media);
    libvlc_audio_set_callbacks(player, playAudio, NULL, NULL, NULL, NULL, NULL);
    libvlc_audio_set_format(player, "FL32", 44100, 2);
    libvlc_video_set_callbacks(player, lockFrame, unlockFrame, NULL, NULL);
    libvlc_video_set_format_callbacks(player, setup, NULL);
    libvlc_media_player_play(player);

    for (int i = 0; i < 50 && libvlc_media_player_get_state(player) != libvlc_Error; i++)
        usleep(100000);

    printf("after 5 seconds: %d video frames, %ld audio samples, state %d\n", frameCount, sampleCount, libvlc_media_player_get_state(player));
    libvlc_media_player_stop(player);
    libvlc_media_player_release(player);
    libvlc_release(instance);
    return (frameCount > 0) ? 0 : 1;
}
EOF

gcc -o "$WORK_DIR/test" "$WORK_DIR/test.c" -I"$ROOT_DIR/Libs/vlc/include" "$WORK_DIR/libvlc.so.5" \
    -Wl,-rpath,'$ORIGIN' -Wl,--disable-new-dtags
VLC_PLUGIN_PATH="$WORK_DIR/plugins" "$WORK_DIR/test" "$VIDEO"
