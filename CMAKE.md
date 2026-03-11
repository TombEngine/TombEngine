# CMake Guide for Windows Contributors

TombEngine uses CMake as the single source of truth for the build.
On Windows you can keep using the Visual Studio solution (`TombEngine.sln`) as before — just follow these steps when you add or remove files.

## How the build is organized

```
TombEngine/
  CMakeLists.txt        # main build rules (rarely needs changes)
  Sources.cmake         # list of every .cpp and .h, grouped by module
TombEngine.sln          # VS solution, kept in sync with Sources.cmake
Tools/sync_vcxproj.py   # script that syncs Sources.cmake <-> vcxproj
```

`Sources.cmake` is the file you edit when adding or removing source files.
The sync script propagates those changes to the `.vcxproj`.

## Adding a new file

### 1. Create the file

Create your `.cpp` / `.h` in the appropriate folder, e.g.:

```
TombEngine/Game/MyFeature/MyFeature.cpp
TombEngine/Game/MyFeature/MyFeature.h
```

### 2. Edit `TombEngine/Sources.cmake`

Open `Sources.cmake` and add the file in the correct section.
The sections are named by module — pick the one that matches the folder:

| Folder prefix          | Source variable              | Header variable              |
|------------------------|------------------------------|------------------------------|
| `Game/`                | `TEN_GAME_SOURCES`           | `TEN_GAME_HEADERS`           |
| `Math/`                | `TEN_MATH_SOURCES`           | `TEN_MATH_HEADERS`           |
| `Objects/`             | `TEN_OBJECTS_SOURCES`        | `TEN_OBJECTS_HEADERS`        |
| `Physics/`             | `TEN_PHYSICS_SOURCES`        | `TEN_PHYSICS_HEADERS`        |
| `Renderer/`            | `TEN_RENDERER_SOURCES`       | `TEN_RENDERER_HEADERS`       |
| `Renderer/Native/DirectX11/` | `TEN_RENDERER_DX11_SOURCES` | `TEN_RENDERER_DX11_HEADERS` |
| `Renderer/Native/OpenGL/`    | `TEN_RENDERER_OPENGL_SOURCES` | `TEN_RENDERER_OPENGL_HEADERS` |
| `Scripting/`           | `TEN_SCRIPTING_SOURCES`      | `TEN_SCRIPTING_HEADERS`      |
| `Sound/`               | `TEN_SOUND_SOURCES`          | `TEN_SOUND_HEADERS`          |
| `Specific/`            | `TEN_SPECIFIC_SOURCES`       | `TEN_SPECIFIC_HEADERS`       |
| `Specific/Platform/`   | `TEN_PLATFORM_SOURCES`       | `TEN_PLATFORM_HEADERS`       |

Add lines keeping **alphabetical order** (case-insensitive, e.g. `Animation.cpp` before `camera.cpp`) and use **forward slashes**. The path must match the actual filename on disk exactly — capitalization matters on Linux:

```cmake
set(TEN_GAME_SOURCES
    ...
    Game/MyFeature/MyFeature.cpp    # <-- add here
    ...
)

set(TEN_GAME_HEADERS
    ...
    Game/MyFeature/MyFeature.h      # <-- add here
    ...
)
```

### 3. Sync the vcxproj

From the repository root, run:

```
python Tools/sync_vcxproj.py --to-vcxproj
```

This updates `TombEngine.vcxproj` to match `Sources.cmake`.

### 4. Verify (optional)

```
python Tools/sync_vcxproj.py --check
```

Prints `OK` if everything is in sync.

## Removing a file

Same process in reverse: delete the lines from `Sources.cmake`, then run `--to-vcxproj`.

## Quick reference

| Task | Command |
|------|---------|
| Sources.cmake -> vcxproj | `python Tools/sync_vcxproj.py --to-vcxproj` |
| vcxproj -> Sources.cmake | `python Tools/sync_vcxproj.py --from-vcxproj` |
| Check sync | `python Tools/sync_vcxproj.py --check` |

## FAQ

**Can I add files directly in Visual Studio instead?**
Yes. Add the file in VS (which modifies the `.vcxproj`), then run `--from-vcxproj` to update `Sources.cmake`. Either direction works — just make sure both files are in sync before committing.

**Do I need CMake installed to build on Windows?**
No. If you only use `TombEngine.sln`, you don't need CMake at all. You just need Python to run the sync script.

**What if I forget to sync?**
CI runs `--check` and will fail if `Sources.cmake` and the `.vcxproj` are out of sync.
