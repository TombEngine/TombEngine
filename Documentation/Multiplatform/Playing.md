# Playing and distributing TombEngine games on Windows, Linux and macOS

A TombEngine game is one folder with the engine and the game data. The engine folder of each platform contains all
the libraries it needs (audio, video playback and their codecs), so players install nothing besides the graphics
drivers of their system.

## Game folder

```
MyGame/
    Bin/x64/                Engine for one or more platforms
        TombEngine.exe      Windows
        TombEngine          Linux or macOS (a Linux and a macOS engine cannot share this folder)
        *.dll / *.so / *.dylib
        plugins/            Video playback plugins
    Shaders/                Compiled shaders of the engine
    Scripts/                Gameflow.lua, Settings.lua, Strings.lua, level and engine scripts
    Data/                   Levels
    Audio/, Sounds/, FMV/, Textures/, Screens/ ...
```

The engine finds the game data in the folder that contains `Scripts/Gameflow.lua`, starting from its own folder and
going up to two levels. A game can therefore ship the same data for every platform, with one `Bin` folder per
platform package.

To build a game folder, copy the content of `Build/Release` of each platform build (see
[Development.md](Development.md)) into the game folder, next to the game data.

## Requirements

| | Windows | Linux | macOS |
|---|---|---|---|
| System | Windows 10 or 11, 64-bit | x86_64 or aarch64 with glibc 2.35 or later (Ubuntu 22.04, Debian 12, Fedora 36, SteamOS 3 or later) | A Mac with Metal, Apple silicon or Intel |
| Graphics | Vulkan, Direct3D 12, DirectX 11 or OpenGL 4.5 driver | Vulkan or OpenGL 4.5 driver (Mesa or vendor) | Metal |
| Audio | Any output device | PipeWire, PulseAudio or ALSA | Any output device |

On Linux the engine also uses the X11 or Wayland, audio and GL libraries of the system, which every desktop
installation has. The glibc requirement can be higher when the engine was built on a newer distribution than the
bundled libraries.

## Running the game

### Windows

Run `Bin\x64\TombEngine.exe`, or the launcher of the game.

### Linux

Run `Bin/x64/TombEngine` from a terminal or a file manager. If the file lost its executable permission while the game
was copied or extracted, restore it with:

```sh
chmod +x Bin/x64/TombEngine
```

#### AppImage

The engine can also be shipped as one `TombEngine-x86_64.AppImage` (or `-aarch64`) file. Place it in the game folder,
next to `Scripts`, `Shaders` and the game data, make it executable and run it: the folder containing the AppImage is
the game folder.

```sh
chmod +x TombEngine-x86_64.AppImage
./TombEngine-x86_64.AppImage
```

The `gamedir <folder>` argument runs a game from another folder. On systems that cannot mount AppImages, run it with
`--appimage-extract-and-run`.

### macOS

Run `Bin/x64/TombEngine` from Terminal or by opening it in Finder. Games downloaded from the internet are quarantined
by Gatekeeper, which blocks the unsigned engine and its libraries; remove the quarantine of the whole game folder once:

```sh
xattr -dr com.apple.quarantine /path/to/MyGame
```

## Settings, saves and logs

| Data | Windows | Linux | macOS |
|---|---|---|---|
| Settings (`ten.conf`) | `%APPDATA%\TEN\TombEngine` | `~/.local/share/TEN/TombEngine` | `~/Library/Application Support/TEN/TombEngine` |
| Saves | `MyGame/Save` | `MyGame/Save` | `MyGame/Save` |
| Logs | `MyGame/Logs` | `MyGame/Logs` | `MyGame/Logs` |

Settings are shared by every TombEngine game of the user. Saves and logs belong to the game, so the game folder must be
writable: on Linux and macOS, do not place it in a system folder.

## Troubleshooting

| Problem | Solution |
|---|---|
| Black screen or no window | Start with another graphics backend: `TombEngine backend opengl` (Linux, Windows), `backend dx11` (Windows). Unavailable backends fall back to the next one; the log names the backend in use. |
| No video in cutscenes or the intro | Check that `Bin/x64/plugins` exists next to the engine and that the video is MP4, MOV, AVI or MKV with a common codec (H.264, HEVC, MPEG-4, VP8, VP9). |
| No sound on Linux | Check that PipeWire or PulseAudio runs; the log warns when no sound device is found. |
| Missing library on Linux | Check the glibc version with `ldd --version`; it must be 2.35 or later. |
| macOS reports a damaged application | Remove the quarantine as shown above. |

Run the engine with the `debug` argument for more details in `Logs`.
