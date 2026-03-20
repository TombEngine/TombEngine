# Cross-Platform Support

This document explains how to build TombEngine from source on Windows, Linux, and macOS.

## Graphics APIs

| API | Windows | Linux | macOS |
|-----|---------|-------|-------|
| **Vulkan** | Default (via SDL_GPU) | Default (via SDL_GPU) | - |
| **Metal** | - | - | Default (via SDL_GPU / MoltenVK) |
| **DirectX 11** | Fallback (`-api dx11`) | - | - |

The default renderer is **Vulkan** (via SDL_GPU) on Windows and Linux, and **Metal** (via SDL_GPU / MoltenVK) on macOS. On Windows, DirectX 11 is available as a command-line fallback:
```bash
TombEngine -api vulkan     # Vulkan (default on Windows/Linux)
TombEngine -api dx11       # DirectX 11 (Windows only)
TombEngine -api metal      # Metal (macOS only)
```

The graphics API cannot be changed from the in-game settings menu.

---

## Third-Party Libraries

All open-source libraries are downloaded and built from source automatically via CMake FetchContent. No manual setup required.

| Library | Version | Notes |
|---------|---------|-------|
| SDL3 | 3.2.8 | Window, input, GPU abstraction |
| spdlog | 1.10.0 | Logging |
| LZ4 | 1.10.0 | Compression |
| Lua | 5.3.6 | Scripting (static library) |
| SDL_shadercross | Pinned commit | HLSL to SPIRV compilation |
| stb | Pinned commit | Image loading/writing (stb_image, stb_image_write) |
| BASS | Prebuilt | Closed-source audio (in `Libs/bass/`) |
| VLC | Prebuilt | Video playback (in `Libs/vlc/`) |
| GLM, sol2, etc. | Vendored | Header-only (in `Libs/`) |
| TBB | System package | Linux/macOS only (`std::execution` support) |

---

## Windows

### What You Need

1. **Visual Studio 2022** or **Visual Studio 2026** with the **"Desktop development with C++"** workload.
2. **CMake 3.21 or newer.** Download the installer from https://cmake.org/download/ and run it. Check "Add CMake to PATH" during installation.

### How to Build (Step by Step)

1. **Clone the repository** (or download and extract the ZIP).

2. **Generate the Visual Studio solution.** In the repository folder, double-click one of these files:
   - `GenerateSolution_VS2022.cmd` (if you have Visual Studio 2022)
   - `GenerateSolution_VS2026.cmd` (if you have Visual Studio 2026)

   A command prompt window will open. Wait until it says "Fatto!" (done). The first run takes a few minutes because it downloads all dependencies.

3. **Open the solution.** Open the generated solution in Visual Studio:
   - VS 2022: **`Build\msvc-2022\TombEngine.sln`**
   - VS 2026: **`Build\msvc-2026\TombEngine.slnx`**

   > **Important:** do NOT open the `TombEngine.sln` in the repository root. That is a legacy file. Always use the CMake-generated solution.

4. **Build.** In Visual Studio:
   - Select **Debug|x64** or **Release|x64** from the toolbar.
   - Press **Ctrl+Shift+B** (or menu Build > Build Solution).

5. **Run.** The compiled executable is in:
   - Debug: `Build\Debug\Bin\Windows\TombEngine.exe`
   - Release: `Build\Release\Bin\Windows\TombEngine.exe`

### Adding New Source Files

1. Add the `.cpp`/`.h` files in Visual Studio as you normally would.
2. Double-click **`SyncSources_FromVcxproj.cmd`** to update the CMake source list.
3. Re-run **`GenerateSolution_VS20xx.cmd`** to refresh the solution.

See [CMAKE.md](CMAKE.md) for more details.

### When to Re-run GenerateSolution

- After `git pull` if `CMakeLists.txt` or `Sources.cmake` changed.
- After adding or removing source files.
- If the build breaks with missing file errors.

You do NOT need to re-run it for normal code changes (editing existing files).

---

## Linux

### What You Need

Install the required packages for your distribution.

**Ubuntu / Debian:**
```bash
sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build \
    libstdc++-dev libgl-dev libtbb-dev wget zstd
```

**Fedora / RHEL:**
```bash
sudo dnf install -y \
    gcc-c++ cmake ninja-build \
    libstdc++-devel mesa-libGL-devel tbb-devel wget zstd
```

**Arch Linux:**
```bash
sudo pacman -S --needed \
    base-devel cmake ninja \
    mesa tbb wget zstd
```

> **Note:** SDL3, spdlog, Lua, LZ4, and SDL_shadercross are downloaded and built automatically. You do NOT need to install them manually.

### How to Build (Step by Step)

1. **Open a terminal** and go to the repository folder:
   ```bash
   cd /path/to/TombEngine
   ```

2. **Configure the build:**
   ```bash
   cmake --preset linux-x64-gcc
   ```
   This downloads all dependencies (first run takes a few minutes).

3. **Build:**
   ```bash
   cmake --build Build/gcc --config Release
   ```

4. **The executable is at:**
   ```
   Build/Release/Bin/Linux/TombEngine
   ```

For a Debug build:
```bash
cmake --build Build/gcc --config Debug
# Output: Build/Debug/Bin/Linux/TombEngine
```

### VLC Libraries (Linux Only)

VLC is bundled as prebuilt shared libraries. During CMake configure, if VLC is not found in `Libs/vlc/linux/<arch>/`, the build system **automatically downloads** it. This requires `wget` and `zstd`.

You can also download manually:
```bash
./Tools/download-vlc-linux.sh           # x86_64 (default)
./Tools/download-vlc-linux.sh aarch64   # ARM64
```

### Setting Up Game Files

The engine expects game data relative to the executable. All platforms share the same folder structure:

```
GameDir/
├── Bin/
│   ├── Windows/
│   │   ├── TombEngine.exe
│   │   └── *.dll
│   ├── Linux/
│   │   ├── TombEngine
│   │   ├── libbass.so, ...
│   │   └── libvlc.so.5, ...
│   └── macOS/
│       ├── TombEngine
│       └── *.dylib
├── Shaders/
├── Scripts/
├── Audio/
├── Data/
├── FMV/
└── Screens/
```

Shared assets (Shaders, Scripts, Audio, Data, FMV) are platform-independent. Shaders and engine scripts are automatically copied by the build system.

### Running

```bash
cd /path/to/GameDir
./Bin/Linux/TombEngine
```

**Without a sound device** (e.g., headless server, CI, WSL without audio):
```bash
SDL_AUDIO_DRIVER=dummy ./Bin/Linux/TombEngine
```

**Command-line options:**

| Option | Description |
|--------|-------------|
| `-debug` | Enable debug mode (shows console) |
| `-level <file>` | Load a specific level file |
| `-api vulkan` | Force Vulkan renderer (default on Windows/Linux) |
| `-api dx11` | Force DirectX 11 renderer (Windows only) |
| `-api metal` | Force Metal renderer (macOS only) |
| `-gamedir <path>` | Set the game asset directory |

### Debug Build with GDB

```bash
sudo apt install -y gdb

cd /path/to/GameDir
SDL_AUDIO_DRIVER=dummy gdb -ex run ./Bin/Linux/TombEngine
```

When a crash occurs, type `bt` in GDB for the backtrace.

### Packaging a Standalone Distribution

#### Tarball (CPack)

```bash
cmake --preset linux-x64-gcc
cmake --build Build/gcc --config Release

# Create .tar.gz
cd Build/gcc && cpack
```

Produces `TombEngine-<version>-linux-<arch>.tar.gz`.

#### AppImage

Requires [appimagetool](https://github.com/AppImage/appimagetool) on `PATH`.

```bash
cmake --preset linux-x64-gcc
cmake --build Build/gcc --config Release
./packaging/appimage/build-appimage.sh Build/gcc
```

### HiDPI / Display Scaling (X11)

On X11 desktops (including WSLg / XWayland), SDL3 cannot reliably detect display scaling. The engine auto-detects it from several sources. If it fails, override manually:

```bash
TEN_HIDPI_SCALE=2.0 ./Bin/Linux/TombEngine
```

On native Wayland, scaling is handled automatically.

### Audio on WSL2 (WSLg)

WSLg includes PulseAudio. To route BASS audio through it:

```bash
sudo apt install libasound2-plugins libpulse0 pulseaudio-utils
```

Create `~/.asoundrc`:
```
pcm.default pulse
ctl.default pulse
```

If audio doesn't work:
```bash
SDL_AUDIO_DRIVER=dummy ./Bin/Linux/TombEngine
```

---

## macOS (Experimental)

### What You Need

```bash
xcode-select --install          # Xcode Command Line Tools
brew install cmake ninja        # CMake and Ninja
```

### How to Build

```bash
cmake --preset macos-clang
cmake --build Build/macos --config Release
```

Libraries are handled the same as Linux: SDL3, spdlog, LZ4, Lua, SDL_shadercross are built from source. BASS and VLC are prebuilt in `Libs/bass/macos/` and `Libs/vlc/macos/`.

---

## CMake Presets Reference

| Preset | Platform | Generator |
|--------|----------|-----------|
| `win-x64-vs2026` | Windows | Visual Studio 18 2026 |
| `win-x64-vs2022` | Windows | Visual Studio 17 2022 |
| `win-x64-ninja` | Windows | Ninja Multi-Config (MSVC) |
| `win-x64-gcc` | Windows | Ninja Multi-Config (GCC/MinGW) |
| `linux-x64-gcc` | Linux | Ninja Multi-Config (GCC) |
| `linux-x64-clang` | Linux | Ninja Multi-Config (Clang) |
| `macos-clang` | macOS | Ninja Multi-Config (Clang) |

All presets output the final executable to `Build/<Config>/Bin/<Platform>/` regardless of which preset is used.
