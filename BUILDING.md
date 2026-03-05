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

Install the required build tools and development libraries.

**Ubuntu / Debian:**

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build pkg-config \
    libgl-dev liblz4-dev liblua5.3-dev libvlc-dev
```

**Fedora:**

```bash
sudo dnf install -y \
    gcc-c++ cmake ninja-build pkg-config \
    mesa-libGL-devel lz4-devel lua-devel vlc-devel
```

> **Note:** SDL3 and spdlog are automatically fetched and built from source via CMake FetchContent. No system packages are needed for these.

### Building

```bash
# Configure (Release)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build
```

The executable is placed at `build/TombEngine/TombEngine`.

### Setting Up Game Files

The engine expects game data files (levels, scripts, audio, etc.) in a directory structure relative to the executable:

```
GameDir/
├── Engine/
│   └── TombEngine          # the executable
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

### Known Issues

- **WSL2 (WSLg):** The Wayland compositor may cause display scaling artifacts with the OpenGL renderer (e.g., tiled/repeated scene). This is a WSLg compositor limitation, not an engine bug. The engine renders correctly on native Linux.
- **Audio on WSL2:** WSL2 does not expose audio devices by default. Use `SDL_AUDIO_DRIVER=dummy` to bypass audio initialization.

---

## macOS

macOS support is experimental. The build system is prepared (CMakeLists.txt handles macOS paths for BASS and VLC libraries), but has not been fully tested. Contributions welcome.
