# Building TombEngine on Windows, Linux and macOS

TombEngine builds on three platforms from the same sources:

| Platform | Toolchain | Project | Graphics backends |
|---|---|---|---|
| Windows x64 | MSVC (Visual Studio 2022 or later) | `TombEngine.sln`, or CMake | Vulkan, Direct3D 12, DirectX 11, OpenGL |
| Linux x86_64 / aarch64 | GCC or Clang | CMake | Vulkan, OpenGL |
| macOS arm64 / x86_64 | Apple Clang | CMake | Metal |

The Visual Studio project is the reference build. `CMakeLists.txt` is generated from it, so it must be regenerated after
files are added to or removed from the project (see [Keeping CMake in sync](#keeping-cmake-in-sync)).

## Folder layout of a build

Every platform writes the same layout under `Build/<Config>`, where `<Config>` is `Debug` or `Release`:

```
Build/<Config>/
    Bin/x64/            Executable, native libraries and the libVLC plugins folder
        plugins/        libVLC plugins (same set on every platform)
    Shaders/            Compiled shaders (SPIR-V, DXIL, DXBC, GLSL, MSL, depending on the platform)
    Scripts/            Engine Lua scripts
```

A playable game folder is this layout plus the game data (levels, scripts, audio, FMV). On Windows and Linux the
Windows and Linux builds can share the same `Build/<Config>` folder: their executables and libraries have different
names (`TombEngine.exe`, `*.dll` against `TombEngine`, `*.so`).

## Bundled native libraries

Libraries without sources in the build are kept as binaries in `Libs` for every platform, so neither developers nor
players install them:

| Library | Version | Folders |
|---|---|---|
| BASS, BASSmix, BASS_FX | 2.4.18, 2.4.13, 2.4.12 | `Libs/bass/{x64,x86,macos,linux/x86_64,linux/aarch64}` |
| libVLC (video playback) | 3.0.23 | `Libs/vlc/{x64,x86,macos,linux/x86_64,linux/aarch64}` |

The pre-build step (`Tools/BuildEvents/PreBuild.ps1`) copies the libraries of the current platform and the libVLC
`plugins` folder next to the executable. The engine points libVLC to that folder through `VLC_PLUGIN_PATH`.

All platforms bundle the same libVLC plugins, listed in `Libs/vlc/plugins.txt`: MP4/MOV, AVI and MKV containers, the
common video and audio codecs through FFmpeg, and the audio and video outputs the engine renders through. To support
another format, add its plugin to the list and run the update scripts below.

### Updating the libraries

| Script | Runs on | Updates |
|---|---|---|
| `Tools/Libraries/UpdateVlcWindows.ps1 [-Version 3.0.23]` | Windows with Visual Studio | Headers, x64 and x86 libVLC and plugins |
| `Tools/Libraries/UpdateVlcMacOS.sh [version]` | macOS, or Linux with `7z` | Universal libVLC and plugins |
| `Tools/Libraries/UpdateVlcLinux.sh [x86_64] [aarch64]` | Linux with `nasm` and `gcc-aarch64-linux-gnu` | libVLC, plugins and their libraries |
| `Tools/Libraries/TestVlcLinux.sh <video>` | Linux | Plays a video with the bundled libVLC and reports the decoded frames |

Windows and macOS use the VideoLAN release packages. Linux uses the libVLC packages of Debian 12, which have the same
release, with a decoder-only FFmpeg 5.1 built by `Tools/Libraries/BuildFfmpegLinux.sh` in place of the Debian one:
this keeps the Linux bundle at about 12 MB instead of more than 130 MB, and needs glibc 2.35 or later.

BASS is updated by hand from the [un4seen](https://www.un4seen.com/) downloads: `bass24`, `bassmix24` and `bass_fx24`,
in their Windows, Linux and macOS packages. Keep the headers in `Libs/bass` and the same version on every platform.

## Windows

Requirements:

- Visual Studio 2022 or later with the *Desktop development with C++* workload.
- The [Vulkan SDK](https://vulkan.lunarg.com/), whose DXC compiles the SPIR-V shaders. The Windows SDK of Visual Studio
  provides FXC (DirectX 11) and the DXC that signs DXIL (Direct3D 12).

Open `TombEngine.sln`, select the `Debug` or `Release` configuration and build. See the main [README](../../README.md)
for the NuGet packages and for creating a game folder from `Build`.

## Linux

Requirements, on Ubuntu 24.04 or a distribution with the same packages:

```sh
sudo bash Tools/CMake/SetupUbuntu.sh
```

The script installs the compilers, CMake, Ninja, the development packages SDL3 is built with (X11, Wayland, audio),
SPIRV-Cross, and the two tools Ubuntu does not package: PowerShell 7 for the build events and DXC for the shaders.
On other distributions install the equivalent packages, [PowerShell 7](https://github.com/PowerShell/PowerShell/releases)
and the [DXC release](https://github.com/microsoft/DirectXShaderCompiler/releases) (or the Vulkan SDK) by hand.

Build:

```sh
cmake -S . -B ~/ten-build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ~/ten-build
```

CMake fetches and builds SDL3, spdlog, lz4 and Lua on the first configuration. The executable is written to
`Build/Debug/Bin/x64/TombEngine`; the CMake folder only holds intermediate files, so it can be anywhere.

The executable finds its libraries through an RPATH to its own folder. It is a classic RPATH rather than a RUNPATH,
because only that also applies to the libraries of the plugins libVLC loads.

### AppImage

```sh
Tools/Packaging/BuildAppImage.sh Release
```

packs `Build/Release/Bin/x64` into `Build/Release/TombEngine-<arch>.AppImage`. The script downloads `appimagetool` on
first use. See [Playing.md](Playing.md) for how players use it.

### WSL

Windows 11 with WSL 2 runs the Linux build with a window (WSLg), audio (PulseAudio of WSLg) and the GPU (Mesa over
Direct3D 12). From Windows, with the repository on `D:`:

```sh
wsl -u root -- bash Tools/CMake/SetupUbuntu.sh
wsl --cd /mnt/d/Repositories/TombEngine -- bash -c "cmake -S . -B ~/ten-build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build ~/ten-build"
wsl --cd /mnt/d/Repositories/TombEngine/Build/Debug/Bin/x64 -- ./TombEngine debug
```

Keep the CMake folder in the Linux file system (`~/ten-build`): building in `/mnt/d` is much slower. Vulkan under WSL
runs on the Mesa *dozen* driver; when it fails, the engine falls back to OpenGL, which can also be chosen with
`backend opengl`.

## macOS

Requirements:

- Xcode command line tools: `xcode-select --install`.
- CMake and Ninja, for example from [Homebrew](https://brew.sh/): `brew install cmake ninja`.
- PowerShell 7: `brew install powershell`.
- The [Vulkan SDK](https://vulkan.lunarg.com/) for macOS, whose DXC and SPIRV-Cross compile the shaders to MSL. The
  shader script finds them through the `VULKAN_SDK` variable the SDK installer sets, or on the `PATH`.

Build:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The build is for the architecture of the Mac; the bundled BASS and libVLC are universal and serve both.

## Keeping CMake in sync

After changing `TombEngine/TombEngine.vcxproj` (files, definitions, include folders), regenerate `CMakeLists.txt`:

```sh
./GenerateCMakeLists.sh     # Linux and macOS, needs PowerShell 7
GenerateCMakeLists.cmd      # Windows
```

Settings of the Linux and macOS builds that have no Visual Studio counterpart are in `Tools/CMake/Dependencies.cmake`.

## Command line

The same arguments work on every platform:

| Argument | Effect |
|---|---|
| `debug` | Debug mode, with the console window on Windows |
| `level <file>` | Starts the given level file instead of the title |
| `gamedir <folder>` | Game folder; by default the folder of `Scripts/Gameflow.lua` up to two levels above the executable |
| `backend <vulkan\|d3d12\|dx11\|opengl\|metal>` | Graphics backend, overriding the configuration; unavailable ones fall back to the next |
| `resolution <width> <height>` | Window size |
| `novsync`, `lowpower` | Present without vertical sync; prefer the integrated GPU |

## Platform-specific files

| Data | Windows | Linux | macOS |
|---|---|---|---|
| Configuration (`ten.conf`) | `%APPDATA%\TEN\TombEngine` | `~/.local/share/TEN/TombEngine` | `~/Library/Application Support/TEN/TombEngine` |
| Saves | `<game>/Save` | `<game>/Save` | `<game>/Save` |
| Logs | `<game>/Logs` | `<game>/Logs` | `<game>/Logs` |

Platform code lives in `TombEngine/Specific/Platform` (`WindowsSubsystem`, `LinuxSubsystem`, `MacOSSubsystem`);
everything else is shared and must not use platform-specific types or functions (see [AGENTS.md](../../AGENTS.md)).
