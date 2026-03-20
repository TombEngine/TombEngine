# CMake Guide for Contributors

TombEngine uses **CMake** as the build system on all platforms.
On Windows, CMake generates a Visual Studio solution that you open and use normally.

## Quick Start (Windows)

1. Install [CMake 3.21+](https://cmake.org/download/) (`.msi` installer).
2. Double-click **`GenerateSolution_VS2022.cmd`** (or `_VS2026.cmd`).
3. Open the generated solution in Visual Studio:
   - VS 2022: **`Build\msvc-2022\TombEngine.sln`**
   - VS 2026: **`Build\msvc-2026\TombEngine.slnx`**
4. Build (Ctrl+Shift+B).

That's it. You never need to type CMake commands manually.

## How the Build is Organized

```
TombEngine/
  CMakeLists.txt          # Build rules and dependency configuration
  Sources.cmake           # List of every .cpp and .h, grouped by module
Tools/
  sync_vcxproj.py         # Syncs Sources.cmake <-> legacy vcxproj
GenerateSolution_VS2022.cmd   # Generates VS 2022 solution
GenerateSolution_VS2026.cmd   # Generates VS 2026 solution
SyncSources_FromVcxproj.cmd   # Updates Sources.cmake from vcxproj
```

## Adding a New Source File

### Option A: Add in Visual Studio (easiest)

1. Add your `.cpp`/`.h` file in Visual Studio as you normally would.
2. Double-click **`SyncSources_FromVcxproj.cmd`** to update `Sources.cmake`.
3. Re-run **`GenerateSolution_VS20xx.cmd`** to refresh the solution.

### Option B: Add in Sources.cmake directly

1. Open `TombEngine/Sources.cmake`.
2. Add the file in the correct section (see table below).
3. Re-run **`GenerateSolution_VS20xx.cmd`**.

### Source File Sections

| Folder prefix                | Source variable                 | Header variable                 |
|------------------------------|--------------------------------|--------------------------------|
| `Game/`                      | `TEN_GAME_SOURCES`             | `TEN_GAME_HEADERS`             |
| `Math/`                      | `TEN_MATH_SOURCES`             | `TEN_MATH_HEADERS`             |
| `Objects/`                   | `TEN_OBJECTS_SOURCES`          | `TEN_OBJECTS_HEADERS`          |
| `Physics/`                   | `TEN_PHYSICS_SOURCES`          | `TEN_PHYSICS_HEADERS`          |
| `Renderer/`                  | `TEN_RENDERER_SOURCES`         | `TEN_RENDERER_HEADERS`         |
| `Renderer/Native/DirectX11/` | `TEN_RENDERER_DX11_SOURCES`    | `TEN_RENDERER_DX11_HEADERS`    |
| `Renderer/Native/SDLGPU/`    | `TEN_RENDERER_SDLGPU_SOURCES`  | `TEN_RENDERER_SDLGPU_HEADERS`  |
| `Scripting/`                 | `TEN_SCRIPTING_SOURCES`        | `TEN_SCRIPTING_HEADERS`        |
| `Sound/`                     | `TEN_SOUND_SOURCES`            | `TEN_SOUND_HEADERS`            |
| `Specific/`                  | `TEN_SPECIFIC_SOURCES`         | `TEN_SPECIFIC_HEADERS`         |
| `Specific/Platform/`         | `TEN_PLATFORM_SOURCES`         | `TEN_PLATFORM_HEADERS`         |

Use **forward slashes** and **alphabetical order**. The path must match the actual filename exactly (case-sensitive on Linux).

## Removing a File

Delete the lines from `Sources.cmake`, then re-run `GenerateSolution_VS20xx.cmd`.

## Sync Script Reference

| Task | Command or Script |
|------|-------------------|
| vcxproj -> Sources.cmake | `SyncSources_FromVcxproj.cmd` (double-click) |
| Sources.cmake -> vcxproj | `python Tools/sync_vcxproj.py --to-vcxproj` |
| Check sync | `python Tools/sync_vcxproj.py --check` |

## FAQ

**Do I need to know CMake to contribute?**
No. Just use the `.cmd` scripts. CMake runs in the background.

**Why can't I just open `TombEngine.sln` from the root?**
The root `.sln` is a legacy file. The CMake-generated solution in `Build\msvc-2022\` (or `Build\msvc-2026\`) has all dependencies configured correctly.

**When do I need to re-run GenerateSolution?**
After pulling changes that modify `CMakeLists.txt`, `Sources.cmake`, or dependency versions. If the build breaks after a `git pull`, re-run the script.

**Can I use a different Visual Studio version?**
Yes. Edit `CMakePresets.json` and change the generator name, or create a new preset. Available generators: `Visual Studio 18 2026`, `Visual Studio 17 2022`, `Visual Studio 16 2019`.
