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
    libgl-dev
```

**Fedora:**

```bash
sudo dnf install -y \
    gcc-c++ cmake ninja-build \
    mesa-libGL-devel
```

> **Note:** SDL3, spdlog, Lua 5.3, and LZ4 are automatically fetched and built from source via CMake FetchContent. No system `-dev` packages are needed for these libraries.

### Downloading VLC Libraries

VLC is bundled as prebuilt shared libraries rather than pulled from system packages. Before building, you must populate `Libs/vlc/linux/<arch>/` using the provided download script:

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

The resulting directory structure:

```
Libs/vlc/linux/x86_64/
├── libvlc.so.5 → libvlc.so.5.6.1
├── libvlc.so → libvlc.so.5.6.1
├── libvlccore.so.9 → libvlccore.so.9.0.1
├── libavcodec.so.58, libavformat.so.58, ...
└── plugins/
    ├── access/
    ├── codec/
    ├── demux/
    └── ...
```

> **Note:** You only need to run this script once. The downloaded libraries are committed to the repository under `Libs/vlc/linux/`, or you can add them to `.gitignore` and download them in CI.

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

**Without a sound device** (e.g., WSL2, headless server, CI):

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

### Known Issues

- **WSL2 (WSLg):** The Wayland compositor may cause display scaling artifacts with the OpenGL renderer (e.g., tiled/repeated scene). This is a WSLg compositor limitation, not an engine bug. The engine renders correctly on native Linux.
- **Audio on WSL2:** WSL2 does not expose audio devices by default. Use `SDL_AUDIO_DRIVER=dummy` to bypass audio initialization.

---

## macOS

macOS support is experimental. The build system is prepared (CMakeLists.txt handles macOS paths for BASS and VLC libraries), but has not been fully tested. Contributions welcome.
