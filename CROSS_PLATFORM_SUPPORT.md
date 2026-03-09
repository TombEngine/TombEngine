# Building TombEngine

This document explains how to build TombEngine from source on Windows and Linux.

## Windows

### Prerequisites

- **Visual Studio 2022** (v143 toolset) with the "Desktop development with C++" workload
- Or **CMake 3.20+** with the Visual Studio generator

All third-party libraries (BASS, Lua, LZ4, SDL3, spdlog, VLC) are vendored in the `Libs/` directory as prebuilt x64 binaries. No additional setup is required.

### Building with Visual Studio

1. Open `TombEngine.sln` in Visual Studio 2022.
2. Select **Debug|x64** or **Release|x64** as the build configuration.
3. Build the solution (**Ctrl+Shift+B**).
4. The executable is output to `Build/<Configuration>/Bin/x64/`.

### Building with CMake

```bash
cmake -B Build/win-x64 -A x64
cmake --build Build/win-x64 --config Release
```

### Selecting the Graphics API

By default, the Windows build includes both DirectX 11 and OpenGL renderers. You can select at runtime:

```bash
TombEngine.exe -api dx11      # DirectX 11 (default)
TombEngine.exe -api opengl    # OpenGL
```

---

## Linux

### Prerequisites

Install the required build tools.

**Ubuntu / Debian:**

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build \
    libgl-dev wget zstd
```

**Fedora / RHEL:**

```bash
sudo dnf install -y \
    gcc-c++ cmake ninja-build \
    mesa-libGL-devel wget zstd
```

**Arch Linux:**

```bash
sudo pacman -S --needed \
    base-devel cmake ninja \
    mesa wget zstd
```

> **Note:** SDL3, spdlog, Lua 5.3, and LZ4 are automatically fetched and built from source via CMake FetchContent. No system `-dev` packages are needed for these libraries.
>
> `wget` and `zstd` are needed by the VLC auto-download script (see below). The script downloads and extracts Ubuntu `.deb` packages using standard tools (`wget`, `ar`, `tar`, `zstd`) — it does **not** require `apt` or Debian/Ubuntu, and works on any Linux distribution.

### VLC Libraries

VLC is bundled as prebuilt shared libraries. During CMake configure, if the VLC libraries are not found in `Libs/vlc/linux/<arch>/`, the build system **automatically downloads** them by running `Tools/download-vlc-linux.sh`. This requires `wget` and `zstd` to be installed (included in the prerequisites above).

You can also run the script manually if needed:

```bash
# Download VLC libs for x86_64 (default)
./Tools/download-vlc-linux.sh

# Or for aarch64
./Tools/download-vlc-linux.sh aarch64
```

The script downloads from Ubuntu 22.04 (jammy) packages and extracts:

| Component | Description |
|-----------|-------------|
| `libvlc.so.*` | VLC main library |
| `libvlccore.so.*` | VLC core library |
| `plugins/` | VLC codec and demux plugins |
| `libavcodec.so.*`, `libavformat.so.*`, etc. | FFmpeg libraries required by VLC plugins |

> **Note:** The VLC libraries are now committed to the repository under `Libs/vlc/linux/`. If you clone fresh, they are already present and no download is needed.

### Building

```bash
# Configure (Release)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build
```

The executable is placed at `build/TombEngine/TombEngine`. BASS and VLC shared libraries are automatically copied next to the binary by the build system.

### Setting Up Game Files

The engine expects game data files (levels, scripts, audio, etc.) in a directory structure relative to the executable:

```
GameDir/
├── Engine/
│   ├── TombEngine           # the executable
│   ├── libbass.so, ...      # BASS shared libraries
│   ├── libvlc.so.5, ...    # VLC shared libraries
│   └── plugins/             # VLC plugins
├── Audio/
├── Data/
├── FMV/
├── Screens/
├── Scripts/
│   ├── Engine/
│   └── SystemStrings.lua
└── Shaders/
```

The `Shaders/` and `Scripts/Engine/` directories are automatically copied by the build system to `build/TombEngine/../Shaders` and `build/TombEngine/../Scripts/`.

### Running

```bash
cd /path/to/GameDir
./Engine/TombEngine
```

**Without a sound device** (e.g., headless server, CI — see [Audio on WSL2](#audio-on-wsl2-wslg) for WSLg setup):

```bash
SDL_AUDIO_DRIVER=dummy ./Engine/TombEngine
```

**Command-line options:**

| Option | Description |
|--------|-------------|
| `-debug` | Enable debug mode (shows console) |
| `-level <file>` | Load a specific level file |
| `-api opengl` | Force OpenGL renderer |
| `-gamedir <path>` | Set the game asset directory |

### Debug Build

```bash
# Configure with debug symbols
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build-debug
```

### Debugging with GDB

Install GDB:

```bash
sudo apt install -y gdb
```

Run with GDB:

```bash
cd /path/to/GameDir
SDL_AUDIO_DRIVER=dummy gdb -ex run ./Engine/TombEngine
```

When a crash occurs, GDB will pause. Type `bt` to see the full backtrace:

```
(gdb) bt
```

Other useful GDB commands:

| Command | Description |
|---------|-------------|
| `bt` | Full backtrace |
| `bt full` | Backtrace with local variables |
| `frame N` | Switch to frame N |
| `print var` | Print variable value |
| `continue` | Resume execution |
| `quit` | Exit GDB |

### Packaging a Standalone Distribution

The build system supports creating a fully self-contained Linux distribution with zero system dependencies beyond glibc and OpenGL drivers. Two formats are available: a tarball and an AppImage.

#### Install Layout

Running `cmake --install` produces the following layout:

```
TombEngine/
├── TombEngine.sh            # launcher script (entry point)
├── Engine/
│   ├── TombEngine           # binary
│   ├── libbass.so, ...      # BASS shared libraries
│   ├── libvlc.so.5, ...    # VLC shared libraries
│   ├── libavcodec.so.58, ...# FFmpeg shared libraries
│   └── plugins/             # VLC plugins
├── Shaders/
└── Scripts/
    ├── Engine/
    └── SystemStrings.lua
```

The `TombEngine.sh` launcher script automatically sets `LD_LIBRARY_PATH` and `VLC_PLUGIN_PATH` so that the bundled libraries are found at runtime. It also passes `-gamedir` pointing to its own directory, so you can place the distribution directly inside a game data folder.

#### Creating a Tarball (CPack)

```bash
# Build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Install to a local directory (for inspection)
cmake --install build --prefix build/install

# Or create a .tar.gz package via CPack
cd build && cpack
```

This produces `TombEngine-<version>-linux-<arch>.tar.gz`. To use it:

```bash
# Extract into a game data directory
cd /path/to/GameDir
tar xf TombEngine-1.0.0-linux-x86_64.tar.gz --strip-components=1

# Run
./TombEngine.sh
```

#### Building an AppImage

An AppImage bundles everything into a single portable executable. It requires [appimagetool](https://github.com/AppImage/appimagetool) on `PATH`.

```bash
# Build first
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build the AppImage
./packaging/appimage/build-appimage.sh build
```

This produces `build/TombEngine-x86_64.AppImage`. Run it from a game data directory:

```bash
cd /path/to/GameDir
./TombEngine-x86_64.AppImage
```

Or specify the game directory explicitly:

```bash
./TombEngine-x86_64.AppImage -gamedir /path/to/GameDir
```

> **Note:** The AppImage contains the engine, shaders, scripts, and all shared libraries. It does **not** contain game data (levels, audio, FMV, etc.) — you must provide those separately.

### HiDPI / Display Scaling (X11)

On X11-based desktops (including WSLg / XWayland), SDL3 cannot reliably detect the compositor's display scale. The engine detects it automatically from several sources, in priority order:

| Priority | Source | Description |
|----------|--------|-------------|
| 1 | `TEN_HIDPI_SCALE` | User override (set to e.g. `2.0`) |
| 2 | `GDK_SCALE` | GNOME integer scale factor |
| 3 | `QT_SCALE_FACTOR` | KDE/Qt fractional scale factor |
| 4 | `~/.config/monitors.xml` | GNOME monitor configuration |
| 5 | `/proc/version` + `reg.exe` | WSLg: reads Windows DPI from the registry |
| 6 | `xrdb -query` → `Xft.dpi` | X11 resource database DPI |
| 7 | `SDL_GetDisplayContentScale` | SDL fallback (often returns 1.0 on X11) |

When a scale > 1.0 is detected, the engine shrinks the window so that the compositor's upscaling produces the intended resolution on screen. The rendering resolution is unaffected.

If automatic detection fails or gives the wrong value, override it manually:

```bash
TEN_HIDPI_SCALE=2.0 ./Engine/TombEngine
```

On native Wayland (not XWayland), the engine disables SDL's built-in scaling and manages resolution internally — no manual override is needed.

### Audio on WSL2 (WSLg)

WSLg includes a PulseAudio server that bridges to Windows audio. BASS (the engine's audio library) uses ALSA, which needs to be configured to route through PulseAudio.

**1. Install the required packages:**

```bash
sudo apt install libasound2-plugins libpulse0 pulseaudio-utils
```

**2. Create `~/.asoundrc` to route ALSA through PulseAudio:**

```
pcm.default pulse
ctl.default pulse
```

**3. Verify audio works:**

```bash
# Check PulseAudio connection
pactl info

# Test audio output
speaker-test -t wav -c 2
```

If `pactl info` fails, check that the PulseAudio socket exists:

```bash
ls -la /mnt/wslg/PulseServer
echo $PULSE_SERVER   # should be unix:/mnt/wslg/PulseServer
```

If the socket is missing, ensure you are running Windows 11 with WSLg enabled and that your WSL distribution is up to date (`wsl --update` from PowerShell).

**Fallback — run without audio:**

```bash
SDL_AUDIO_DRIVER=dummy ./Engine/TombEngine
```

The engine will detect the missing audio device and disable sound automatically, but suppressing the ALSA errors requires the `dummy` driver.

### Known Issues

- **Screen resolution list on WSLg:** The virtual X11 display in WSLg reports a scaled-down resolution. The engine compensates using the detected HiDPI scale, but the list may not include all native modes. Use `TEN_HIDPI_SCALE` if the maximum resolution appears too low.
- **Wayland backend:** Forcing `SDL_VIDEO_DRIVER=wayland` is not supported on WSLg — the Wayland backend may crash or fail to initialize. Let SDL choose the default (`x11` on WSLg).

---

## macOS

macOS support is experimental. The build system is prepared (CMakeLists.txt handles macOS paths for BASS and VLC libraries), but has not been fully tested. Contributions welcome.
