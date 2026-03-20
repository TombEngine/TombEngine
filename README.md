# Tomb Engine

![Logo](https://github.com/TombEngine/TombEngine/blob/7c50d26ca898c74978336d41e16ce3ce0c8ecacd/TEN%20logo.png)

*Tomb Engine* (*TEN*) is an open-source custom level engine which aims to abolish limits and fix bugs of the classic Tomb Raider games. It aims to introduce new features, refine old ones, and provide a user-friendly level creation process. Current support includes:
- *Lua* as the native scripting language.
- Many objects from the original series (1-5).
- Support for high framerate, antialiasing, mipmapping, and SSAO.
- Full diagonal geometry support.
- Uncapped map size.
- A streamlined player control scheme.

## Graphics APIs

| API | Windows | Linux | macOS |
|-----|---------|-------|-------|
| DirectX 11 | Fallback (`-api dx11`) | - | - |
| Vulkan | Default (via SDL_GPU) | Default (via SDL_GPU) | - |
| Metal | - | - | Default (via SDL_GPU / MoltenVK) |

*Tomb Engine* is used in conjunction with *Tomb Editor*. The repository can be found [here](https://github.com/MontyTRC89/Tomb-Editor).

## Building from Source

### Windows (Visual Studio)

**Requirements:**
- [Visual Studio 2022](https://visualstudio.microsoft.com/) or [Visual Studio 2026](https://visualstudio.microsoft.com/) with the "Desktop development with C++" workload.
- [CMake 3.21+](https://cmake.org/download/) (download the `.msi` installer and install).

**Steps:**

1. Clone the repository.
2. Double-click **`GenerateSolution_VS2022.cmd`** (or `GenerateSolution_VS2026.cmd`).
   - This downloads and configures all dependencies automatically.
   - Wait for it to finish (first run takes a few minutes).
3. Open the generated solution in Visual Studio:
   - VS 2022: **`Build\msvc-2022\TombEngine.sln`**
   - VS 2026: **`Build\msvc-2026\TombEngine.slnx`**
4. Select **Debug|x64** or **Release|x64**.
5. Build the solution (Ctrl+Shift+B).
6. The executable appears in `Build\Debug\Bin\Windows\` (or `Build\Release\Bin\Windows\`).

> **Adding new source files:** add them in Visual Studio normally, then double-click **`SyncSources_FromVcxproj.cmd`** to update the CMake file list. Re-run `GenerateSolution_VS20xx.cmd` to refresh the solution.

### Linux

See [CROSS_PLATFORM_SUPPORT.md](CROSS_PLATFORM_SUPPORT.md) for detailed instructions.

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install build-essential cmake ninja-build libstdc++-dev libgl-dev libtbb-dev wget zstd

# Build
cmake --preset linux-x64-gcc
cmake --build Build/gcc --config Release
```

### macOS (experimental)

See [CROSS_PLATFORM_SUPPORT.md](CROSS_PLATFORM_SUPPORT.md).

## Selecting the Graphics API at Runtime

The default renderer is **Vulkan** (via SDL_GPU) on Windows and Linux, and **Metal** (via SDL_GPU / MoltenVK) on macOS. On Windows, DirectX 11 is available as a fallback:

```bash
TombEngine -api vulkan     # Vulkan (default on Windows/Linux)
TombEngine -api dx11       # DirectX 11 (Windows only)
TombEngine -api metal      # Metal (macOS only)
```

## Contributions

Contributions are welcome. If you would like to participate in development to any degree, whether that be through suggestions, bug reports, or code, join our [Discord server](https://discord.gg/h5tUYFmres).

See [CONTRIBUTING.md](CONTRIBUTING.md) for coding conventions and workflow.

## Disclaimer

Tomb Engine uses modified MIT license for non-commercial use only. For more information, see [license](https://github.com/TombEngine/TombEngine?tab=License-1-ov-file#readme). Tomb Engine is unaffiliated with the Crystal Dynamics group of companies or Embracer Group AB. *Tomb Raider* is a trademark of the Crystal Dynamics group of companies. Tomb Engine team is not responsible for illegal use of this source code and built binaries alone or in combination with third-party assets or components. This source code is released as-is and continues to be maintained by non-paid contributors in their free time.
