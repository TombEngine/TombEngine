# TombEngine Renderer Architecture

This document describes the renderer of TombEngine as it is after the retirement of the legacy DirectX 11 renderer:
how a frame is prepared and drawn, how level data reaches the GPU, how the backends differ, and how to measure and
debug it. It ends with the reference of every benchmarking command.

Related documents in this folder:

- `TransparentSorting.md`: collection, grouping and sorting of the transparent faces.
- `ModernizationPlan.md` (Italian): planned work on performance, memory and assets.
- `Benchmark.lua`, `Sweep.lua`, `InventoryTest.lua`: benchmark scenario scripts.
- `RendererParity.ps1`: visual regression test against saved reference images.

---

## 1. Overview

The renderer has three layers:

| Layer | Class / interface | Folder | Role |
|---|---|---|---|
| Frontend | `Renderer` (`g_Renderer`) | `Renderer/`, `Renderer/Scene`, `Renderer/UI`, `Renderer/Sky` | API used by the game code. Decides what is drawn: portal culling, item, static, light and effect collection, sorted transparent faces, sky effect state, 2D layer. Runs on the CPU only. |
| Backend | `SceneRenderer` (`g_SceneRenderer`) | `Renderer/Backend` | Owns every GPU resource. Uploads the level at load, builds per-frame GPU data and records the passes of a frame. |
| RHI | `RHI::IDevice` and friends | `Renderer/RHI` | Small rendering hardware interface modeled on SDL GPU, with DirectX 11, SDL GPU (Vulkan and Direct3D 12) and OpenGL 4.5 implementations. |

Design rules:

- **One renderer, four backends.** Vulkan is the default and the main optimization target; DirectX 11, Direct3D 12
  and OpenGL 4.5 implement the same RHI and run the same backend code.
- **Persistent GPU data.** Level geometry and textures are uploaded once. Per frame, only instances, lights, bones,
  materials, sprites, decals, sorted indices and uniforms are uploaded, all through one upload ring.
- **Instanced, state-sorted draws.** Objects are drawn by instance index; draws are sorted by pipeline, texture array
  group and material so state changes are rare.
- **Offline shaders.** All shaders are compiled at build time; pipelines a level can use are created at load.
- **Named everything.** Every pass, resource and pipeline has a readable debug name for RenderDoc, Nsight and RGP.
- **Classic look by default.** Modern features (SSAO, glow, reflections, water surfaces, volumetric sky) are additive
  and can be turned off.

---

## 2. Source layout

| Path | Content |
|---|---|
| `Renderer/Renderer.h`, `Renderer.cpp`, `RendererInit.cpp`, `RendererSettings.cpp` | `Renderer` class, initialization, screen and settings changes. |
| `Renderer/RendererEnums.h`, `RendererUtils.*` | Constants, enums and helpers shared by frontend and game code. |
| `Renderer/Scene/` | Frontend of the 3D scene. `RendererFrame.cpp` (room and portal culling, item, static, light and effect collection), `RendererDraw.cpp` (frame entry points, blob shadows, `SubmitFrame`, sorted face collection), `RendererDrawEffect.cpp` (particles and effects as sprites or meshes), `RendererLara.cpp`, `RendererSorted.cpp` (transparent sorting), `RendererSprites.cpp`, `RendererPostProcess.cpp` (post-process and depth of field state), `RendererHelper.cpp`, `RendererCompatibility.cpp` (level conversion), `RenderView.*`, `Frustum.*`. |
| `Renderer/UI/` | 2D layer: strings, fonts, HUD bars, menus, inventory, display sprites, debug overlay and ImGui windows. |
| `Renderer/Sky/` | CPU state of the sky effects: atmospheric dome, sun, moon, aurora, underwater sky, volumetric clouds, god rays, dust storm, their settings and quality levels. |
| `Renderer/Structures/` | Frontend data: rooms, buckets, polygons, meshes, objects, items, statics, lights, sprites, sortable objects, the level vertex. |
| `Renderer/ConstantBuffers/` | Uniform blocks of the sky effects (atmospheric sky, god rays, dust storm, volumetric clouds), filled by the frontend and pushed by the backend as pixel uniforms. |
| `Renderer/Backend/` | `SceneRenderer`, one file per area: `SceneRenderer.cpp` (initialization, frame, scene pass, objects, sorted faces), `SceneRendererGeometry.cpp`, `SceneRendererTextures.cpp`, `SceneRendererPipelines.cpp`, `SceneRendererShadows.cpp`, `SceneRendererSSAO.cpp`, `SceneRendererSky*.cpp`, `SceneRendererAtmosphere.cpp`, `SceneRendererWater.cpp`, `SceneRendererLightClusters.cpp`, `SceneRendererMirrors.cpp`, `SceneRendererSprites.cpp`, `SceneRendererEffects.cpp`, `SceneRendererDebris.cpp`, `SceneRendererDOF.cpp`, `SceneRendererGlow.cpp`, `SceneRendererPostProcess.cpp`, `SceneRendererSMAA.cpp`, `SceneRendererUpscale.cpp`, `SceneRendererOverlay.cpp`, `SceneRendererImGui.cpp`, `SceneRendererVideo.cpp`, `SceneRendererDebug.cpp`, `SceneRendererStats.cpp`; `GpuTypes.h`, `OverlayTypes.h`, `SkyTypes.h`, `DrawSort.*`, `TextureLoader.*`. |
| `Renderer/RHI/` | `RHIDevice.h` (interfaces), `RHITypes.h` (descriptions and enums), `DirectX11/`, `SDLGPU/`, `OpenGL/` devices, and helpers: `ShaderLibrary` (loading compiled shaders), `UploadRing`, `RenderTargetPool`, `TextureCapture` (readback and screenshots), `MemoryTracker`. `Test/` holds a standalone device test. |
| `Shaders/` | HLSL sources and `CompileShaders.ps1`. `GpuShared.hlsli` holds the structures shared with C++, `ShaderInterop.hlsli` the binding macros of each backend. |
| `Specific/Benchmark.*` | Benchmark mode: CSV recording, warmup, screenshots, automatic exit. |
| `Specific/EngineMain.cpp` | Command line, backend selection and fallback, window creation, game and window threads. |

File names follow the class they implement: `Renderer<Topic>.cpp` files hold methods of `Renderer`,
`SceneRenderer<Topic>.cpp` files hold methods of `SceneRenderer`.

---

## 3. Threads and frame lifecycle

TombEngine runs the game logic and the renderer frontend on the **game thread**; the main thread owns the window and
the event loop (**window thread**).

1. The game calls `g_Renderer.Render()` (or a menu, inventory, loading or video entry point).
2. `Renderer::RenderScene()` builds a `RenderView`:
   - `CollectMirrors`, `CollectRooms`: rooms visible through portals, with their portal rectangles; items, statics,
     effects, lights and decals of each visible room.
   - Animation update of the player and items, camera lights, blob shadow spheres.
   - `PrepareSceneSprites`: particles, smoke, fire, sparks, blood, weather and other sprite effects.
   - `PrepareSkyEffects`: draw records and uniform blocks of the sky effects.
   - `CollectSortedFaces` and `SortTransparentFaces`: transparent faces in back to front order.
   - `CollectHud`: HUD objects, bars, strings and display sprites into the `UiFrame`.
3. `Renderer::SubmitFrame()` fills a `Backend::FrameEnvironment` with the values that are not part of the view (fog,
   frame counters, post-process, depth of field, lens flares, sky draws, shadows, debug primitives, video frame) and
   calls `g_SceneRenderer.RenderFrame(view, environment)`.
4. `SceneRenderer::RenderFrame()` waits for the oldest frame in flight (`FRAMES_IN_FLIGHT_MAX` = 2 fences), collects
   the GPU data of the frame, uploads it in one copy pass and records the passes of `FRAME_PASSES` (section 5).
5. The frontend finishes the 2D layer and calls `SceneRenderer::FinishFrame(ui)`, which composes the overlay over the
   scene into the final target and submits.
6. Presentation: DirectX 11 and OpenGL present on the game thread. SDL GPU presents on the window thread
   (`PresentOnWindowThread()`), with three swapchain frames in flight.
7. With high framerate on, frames interpolate between the 30 Hz game ticks. On SDL GPU with vertical sync,
   `Renderer::PaceFrame()` paces the game thread to the refresh rate, since mailbox presentation does not block.

OpenGL keeps one context current on one thread: a thread that stops rendering calls `SceneRenderer::ReleaseThread()`
(at game thread start and exit and around the asynchronous level load). A new rendering thread needs the same
hand-off.

---

## 4. Frontend

### 4.1 Culling

Rooms are culled through portals (`GetVisibleRooms`, `CheckPortal`): each visible room receives the screen rectangle
of the portals it is seen through (`RendererRoom::ViewPort`). Items and statics are tested against the frustum, lights
are collected per room and per item (`CollectLights`, up to `MAX_LIGHTS_PER_ITEM` for objects), fog bulbs and lens
flares per view. Mirrors are collected as reflection planes; the backend adds reflected copies of the player, moveables, statics, sprites
and sorted faces of the mirror room, as each mirror allows.

### 4.2 Transparent faces

Faces with blend modes that do not commute (alpha blend, subtractive, screen, lighten, exclude and faded objects) are
collected per polygon by `CollectSortedFaces` and the `CollectSorted*Faces` functions, grouped into clusters of
overlapping objects and sorted with a radix sort. Sprites with these blend modes join the same list. See
`TransparentSorting.md`.

### 4.3 Sky effects

The sky files of the frontend advance the state of the volumetric clouds, aurora, moon, god rays and dust storm, and
fill the uniform blocks and draw records (`CloudLayerDraw`, `GodRayDraw`, `HorizonLayerDraw`, `DustStormDraw`) that the
backend replays. The cloud sun occlusion of the lens flares is read back from the GPU some frames later
(`ReadCloudTransmittance`).

### 4.4 2D layer

Strings, bars, images, display sprites, inventory and menu objects are recorded into a `UiFrame` (`OverlayTypes.h`).
Menu and inventory objects are 3D meshes drawn by the backend in their own pass with depth.

### 4.5 Level conversion

`Renderer::PrepareDataForTheRenderer()` (`Scene/RendererCompatibility.cpp`) converts the level into renderer rooms,
buckets, polygons, meshes, objects and sprites, and vertex and index arrays for rooms, statics and moveables. Large
room buckets are split into spatial chunks (recursive median splits) so the backend culls them per chunk against the
portal rectangles. The result is handed to the backend as a `Backend::LevelGeometry`.

---

## 5. Backend

### 5.1 Level resources

`SceneRenderer::LoadLevel()`:

- **Geometry** (`SceneRendererGeometry.cpp`): rooms, statics and moveables share one vertex buffer and one index
  buffer, in sections (`GeometrySection`). The CPU index copies stay for the sorted faces.
- **Textures** (`SceneRendererTextures.cpp`): see section 6.
- **Animated textures**: frames and sets in storage buffers, selected per draw; waterfalls and video textures are
  animated sets too.
- **Water surfaces** (`SceneRendererWater.cpp`): see section 6.4.
- **Pipelines** (`SceneRendererPipelines.cpp`): every scene pipeline the level can bind is created at load, so no
  pipeline is compiled during gameplay.

### 5.2 Per-frame data

Collected on the CPU, uploaded once per frame through `RHI::UploadRing` before the passes:

| Buffer | Structure (`GpuShared.hlsli`) | Content |
|---|---|---|
| Instances | `GpuObjectInstance` | World matrix, color, ambient light, light range, bone offset and skinning mode of each drawn object. |
| Instance indices | vertex stream | Instance number per draw instance. |
| Lights | `GpuLight` | Lights of rooms and objects; objects reference a range. |
| Light clusters | masks | Room light masks per cluster of the view frustum (`SceneRendererLightClusters.cpp`): screen tiles times logarithmic depth slices, so room pixels evaluate only nearby dynamic lights. |
| Bones | matrices | Animation transforms of moveables and the player. |
| Materials | `GpuMaterial` | Material values of the level, re-uploaded when they change. |
| Decals | `GpuDecal` | Decals per room. |
| Sprites | `GpuSprite` | Billboards and quads of the frame, drawn instanced. |
| Sorted indices | 32 bit indices | Indices of the transparent faces in sorted order. |
| Debris, debug, overlay, ImGui | vertices | Dynamic geometry of these features. |

Uniform blocks (`FrameVertexUniforms`, `FramePixelUniforms`, per-draw uniforms) are pushed with
`ICommandBuffer::PushUniforms`; the RHI stores them in a ring on every backend.

### 5.3 Frame passes

`SceneRenderer::FRAME_PASSES` lists the passes in order. Each runs in its own debug group; a pass with a flag is
skipped when the flag is false for the frame.

| # | Pass | Runs when | Output | Notes |
|---|---|---|---|---|
| 1 | Shadow map | A shadow casting light is active | Cube shadow map (R32F, size from the settings) | Six faces; casters culled per face. |
| 2 | Depth and normals prepass | Always | Depth (D32F), normals, R32F depth copy, emissive | Room and object prepass shaders; normal maps only with SSAO. |
| 3 | SSAO | Ambient occlusion on | Half resolution occlusion | Separable blur. |
| 4 | Sky reflections | Sky reflective materials or water reflecting a visible sky | Dual paraboloid sky map | One hemisphere per layer. |
| 5 | Distortion | Distortion draws in the view | Distortion target | Applied by the post-process. |
| 6 | Sky | Always | Scene target | Classic sky layers, atmospheric dome, sun, moon, aurora, stars, volumetric clouds, horizon meshes, god rays. Occluded sky is not shaded. |
| 7 | Scene | Always | Scene target | Opaque rooms (portal scissors, per-chunk culling) and objects, alpha edges, water surfaces and underwater fog, additive draws, debris, gun flashes, mirrors, sorted transparent faces, dust storm, debug primitives. |
| 8 | DOF | Depth of field active | Near and far blur layers | Unused layers are skipped. |
| 9 | Reflections | Reflective materials | Half resolution copy of the scene | Read by reflective materials in the next frame. |
| 10 | Glow | A visible emissive material | Downscaled, blurred emissive | Added by the post-process. |
| 11 | Post-process | Always | Color target | Distortion, glow, tint, color scheme, gamma, fade, cinematic bars, lens flares; then FXAA (low) or SMAA (medium, high). |
| 12 | Upscale | Render scale below native | Window resolution color target | AMD FSR 1 EASU, with RCAS applied while the overlay draws the scene. |

`FinishFrame` then draws the 2D layer (quads, text, bars, 3D menu objects with their own post-process) over the scene
into the final target, and ImGui on top in debug mode.

### 5.4 Render targets

Scene targets (scene color, depth, normals, depth copy, emissive) have the render resolution, the window size times the
render scale; the color and final targets, the overlay and the upscale output have the window size. Intermediate
targets come from `RHI::RenderTargetPool` and are reserved with the frame targets, so they are allocated before the
level fills the video memory (on Vulkan, late allocations can land in system memory).

### 5.5 Presentation and pacing

With vertical sync, SDL GPU presents in mailbox mode where available and the game thread is paced to the refresh
rate; without it, immediate mode is used. DirectX 11 uses the flip model and tears without vertical sync. Frame fences
keep the game thread at most two frames ahead of the GPU on every backend.

---

## 6. Assets

### 6.1 Level textures

Every level texture page has up to four maps: color, normal, ORSH (ambient occlusion, roughness, specular, height)
and emissive. Tomb Editor writes each map as an encoded blob in the `.ten` file. The engine does not compress or
convert textures at load:

- **Texture compression on in Tomb Editor (level setting, optional):** maps are DDS files with the full mip chain,
  written by DirectXTex. Color, ORSH and emissive maps are BC1 when every texel is opaque or transparent black (1 bit
  alpha), BC3 otherwise; normal maps are BC5 (the shader rebuilds Z). Tomb Editor chooses BC1 or BC3 and packs
  BC1-compatible areas on their own atlas pages. These files are uploaded as they are.
- **Texture compression off:** maps are PNG files, decoded to RGBA8 with mips generated on the GPU. They work but use
  four to eight times the video memory; the log reports how many maps are not block compressed.

Accepted DDS formats: legacy FourCC (DXT1, DXT3, DXT5, ATI1/BC4U, ATI2/BC5U, uncompressed 32 bit) and DX10 headers
with UNORM DXGI formats (RGBA8, BGRA8, BC1 to BC5, BC7). sRGB variants are not accepted. Data must hold every mip
level declared in the header, largest first.

Pages are grouped into texture arrays by size, format and mip count of all four maps (`ArrayGroupKey`); a draw selects
its layer with a per-draw uniform. Pages without a map use one-layer default arrays (white, flat normal, default ORSH,
black). When the textures do not fit the free video memory of a discrete GPU, top mips are left out (emissive first,
then ORSH, normal and color); integrated GPUs keep every mip. Encoded blobs are freed after the upload.

Sprite atlases stay single textures, since the 2D layer samples them too. The sky texture uses its color map only.

### 6.2 Other textures

Fonts (`.spritefont`), menu images, loading screens and the logo are PNG, JPEG or DDS files decoded by
`TextureLoader` (PNG through SDL, JPEG through stb_image). Video frames are uploaded as BGRA8 each frame. Noise
textures (SSAO, clouds, SMAA, water waves) are generated or embedded.

### 6.3 Materials and shader variants

Room and object pixel shaders are compiled with options that remove the normal, ORSH and emissive map reads and the
alpha test; the backend picks the variant per draw from the maps the texture group lacks, so shaders have no
branches for missing maps.

### 6.4 Water surfaces (temporary)

Classic levels have no water surface geometry: the surfaces are the horizontal portals between a water room and the
dry room above it. At load, `SceneRendererWater.cpp` builds a displaced grid over each such portal, hides the
builder-made faces that fake a surface there, and draws the surfaces in the scene pass with refraction, reflection
(traced at half resolution) and swells; underwater fog absorbs the light below the surface.

**This computation lives in the engine temporarily.** It will move to Tomb Editor, which will write the water
surfaces into the level file; the engine will then only draw them.

---

## 7. RHI

`RHIDevice.h` declares `IDevice`, `ICommandBuffer`, `IRenderPass`, `ICopyPass`, `IComputePass`, resources
(`IBuffer`, `ITexture`, `ISampler`, `IShader`, pipelines, `IFence`, `ITransferBuffer`) and their descriptions
(`RHITypes.h`). The model follows SDL GPU: immutable pipelines, explicit render, copy and compute passes, transfer
buffers for uploads, cycling of resources rewritten every frame, and uniform pushes.

| Backend | Device | Shaders | Notes |
|---|---|---|---|
| Vulkan | `SDLGPU/SDLGPUDevice.cpp` (SDL3 GPU) | SPIR-V (DXC) | Default. Presents on the window thread, three swapchain frames in flight. No timestamp queries: GPU timings split the command buffers (see 9.4). |
| Direct3D 12 | `SDLGPU/SDLGPUDevice.cpp` (SDL3 GPU) | DXIL (DXC of the Windows SDK) | Register spaces 0 to 3 like the SPIR-V sets; storage buffers read as byte address buffers. |
| DirectX 11 | `DirectX11/DX11Device.cpp` | DXBC (FXC) | Feature level 11.0; uniforms in a ring bound by offset with D3D11.1, one buffer per slot otherwise. Timestamp queries. |
| OpenGL 4.5 | `OpenGL/GLDevice.cpp` | GLSL (SPIRV-Cross from the SPIR-V) | Separate shader objects; bindings offset per stage; upper-left clip origin and 0 to 1 depth, the image is flipped when copied to the window. Timestamp queries. Context current on one thread at a time. |

Selection: the configured backend (`graphics_backend` in the configuration, 1 Vulkan, 2 DirectX 11, 3 Direct3D 12,
4 OpenGL, cycled in the graphics menu and applied at the next start) or the `backend` command-line argument. If it
cannot start, the others are tried in the order Vulkan, Direct3D 12, DirectX 11, OpenGL. The value 0 of older
configurations (the retired legacy renderer) is read as Vulkan.

Shared helpers: `UploadRing` (linear allocator over a cycled transfer buffer, one copy pass per frame),
`RenderTargetPool`, `TextureReadback` (non-blocking readback for screenshots and cloud occlusion), `MemoryTracker`
(GPU memory per category and the process video memory budget from DXGI).

---

## 8. Shaders

- Sources are HLSL in `Shaders/`. Each `.hlsl` declares its entry points and resource counts in `// TEN_SHADER:`
  lines; `permutations=` adds one variant per define, `options=` a variant for every combination of its defines.
- `CompileShaders.ps1` runs as a pre-build step and writes `Build/<Configuration>/Shaders/Bin`: DXBC with FXC (DX11),
  SPIR-V with the DXC of the Vulkan SDK (Vulkan), DXIL with the DXC of the Windows SDK (Direct3D 12), GLSL with
  SPIRV-Cross (OpenGL), plus a `.meta` file of resource counts per shader. A missing compiler only disables its
  backend; at least FXC or the Vulkan DXC is required.
- `ShaderInterop.hlsli` maps uniform blocks, storage buffers and texture-sampler pairs to the binding model of each
  backend (`TEN_PS_UNIFORMS`, `TEN_VS_UNIFORMS` and similar macros). `GpuShared.hlsli` declares the structures
  shared with C++, included by `Backend/GpuTypes.h`, so layouts are written once.
- All shaders are Shader Model 5.0 compatible.
- In a development checkout, **F9** (debug mode) recompiles the shader sources of the repository and reloads them
  (`SceneRenderer::ReloadShaders`).

---

## 9. Debugging and statistics

### 9.1 Debug mode keys

Start the engine with `debug` (or a Debug build) to enable:

| Key | Action |
|---|---|
| F8 | Toggle the performance panel: frame time history, backend CPU time, GPU time per pass (top passes), video memory. |
| F9 | Recompile and reload the shaders. |
| F10, F11 | Next and previous debug page (Renderer: statistics, portals, wireframe; Player; Collision; Pathfinding; Sky). |
| Tab | Cycle the pathfinding display on the pathfinding page. |

The renderer statistics page shows the frontend counts (rooms, sprites, lights, fog bulbs, sorted faces, collect and
sort times), the backend counts (draw calls per category, instances, uploads, API calls, sorted draws), and the device,
targets and level resources (`SceneRendererInfo`). GPU pass timings are measured only while the page or the
performance panel requests them.

### 9.2 Logs

At exit the backend logs the GPU time per pass (when measured) and the GPU memory by category with the process video
memory budget. The log is `Logs/TENLog_<timestamp>.txt` in the game directory.

### 9.3 External tools

Every pass is a debug group and every resource and pipeline is named: RenderDoc works on all backends; Nsight and
Radeon GPU Profiler work on Vulkan and Direct3D 12. On hybrid laptops the renderer requests the high performance GPU;
use `lowpower` to test the integrated one.

### 9.4 GPU timings

DirectX 11 and OpenGL measure passes with timestamp queries. SDL GPU has none, so while timings are requested the
command buffer is split where the top-level debug groups begin and end, and a thread times the fence of each part.
The split itself costs some GPU time; use `nogputiming` to benchmark SDL GPU without it.

### 9.5 Device test

`rhitest <backend>` draws a standalone test scene through the RHI for 120 frames, saves a screenshot to
`Logs/RHITest_<Backend>.png` and quits. It validates a backend without loading the game.

---

## 10. Benchmarking

### 10.1 Command line

Run from the game directory (for example `Build/Debug`), or pass `gamedir <path>`:

```
Build\<Config>\Bin\x64\TombEngine.exe debug level Data/<level>.ten benchmark <seconds> [options]
```

Arguments are case insensitive and may start with `-`, `--` or `/`.

| Argument | Values | Effect |
|---|---|---|
| `debug` | | Debug mode: console, debug keys and overlays. Also keeps the game running when the window loses focus. |
| `level <file>` | `Data/<level>.ten` | Starts directly in this level. The level must be listed in `Scripts/Gameflow.lua`. |
| `gamedir <path>` | directory | Game directory, if not the working directory or its parents. |
| `benchmark <seconds>` | float | Benchmark mode for this duration after a 3 second warmup (10.2). |
| `backend <name>` | `vulkan`, `dx11`, `d3d12`, `opengl` | Renderer backend for this run, overriding the configuration. Falls back to the others if it cannot start. |
| `novsync` | | Presents without vertical sync (immediate mode), to measure uncapped frame times. |
| `nogputiming` | | Disables GPU pass timings during the benchmark. On SDL GPU (Vulkan, Direct3D 12) timings split the command buffers and cost GPU time. |
| `renderscale <0-4>` | 0 native, 1 ultra quality (1/1.3), 2 quality (1/1.5), 3 balanced (1/1.7), 4 performance (1/2) | Render scale for this run; below native, FSR 1 upscales the scene. |
| `ssao <0-1>` | | Ambient occlusion off or on for this run. |
| `antialiasing <0-3>` | 0 none, 1 FXAA, 2 SMAA medium, 3 SMAA high | Antialiasing for this run. |
| `resolution <width> <height>` | pixels | Windowed resolution for this run. |
| `lowpower` | | Runs on the integrated GPU of hybrid systems. |
| `rhitest <backend>` | as `backend` | Runs the RHI device test instead of the game (9.5). |
| `hash <number>` | | System name hash used by the level checks. |

The overrides (`renderscale`, `ssao`, `antialiasing`, `resolution`) apply to the run only, but the game writes its
configuration on exit, so back up `ten.conf` (or restore the options in the menu) after a benchmark session.

### 10.2 Benchmark mode

With `benchmark <seconds>`:

- Timing starts with the first rendered gameplay frame (level loading is excluded); the first 3 seconds are discarded.
- Every frame of the measured period is written to `Logs/Benchmark_<yyyymmdd_hhmmss>.csv`.
- A summary is written to the log at the end: frame time average, p50 and p99, render time average and p99, average
  draw calls and uniform bytes.
- The game quits when the duration elapses.
- A screenshot of the scene (without overlays) is saved at game frame 240 as `Logs/Benchmark.png`, then every 240
  frames as `Logs/Benchmark_<n>.png` (one per profiling flyby sequence). The frames around each capture are excluded
  from the CSV.
- In the title menu or the inventory, which do not run the game loop, the menu is captured after 90 logic ticks and
  the run ends after 150 ticks.
- The game keeps running when the window loses focus, so the measurement is not interrupted.

CSV columns:

| Column | Meaning |
|---|---|
| `frame`, `time_s` | Frame number and time since the end of the warmup. |
| `cpu_frame_ms` | Time between consecutive presented frames. |
| `cpu_render_ms` | CPU time in the frontend before presenting, without the backend recording. When the GPU is the bottleneck it can include waits. |
| `present_ms` | Time blocked in present (vertical sync, pacing, GPU). |
| `rooms`, `sorted_faces` | Rooms drawn and sorted polygon entries. |
| `frontend_sorted_collect_ms`, `frontend_sorted_sort_ms` | Frontend collection and sort of the transparent faces. |
| `backend_cpu_ms` | Backend CPU time: collection, upload and command recording. |
| `draw_calls`, `shadow_draw_calls`, `triangles` | Draws of the frame and of the shadow map, triangles drawn. |
| `uniform_pushes`, `uniform_bytes`, `upload_bytes` | Uniform pushes, their bytes, and bytes uploaded through the upload ring. |
| `sorted_collect_ms`, `sorted_record_ms` | Backend time building and recording the sorted runs. |
| `gpu_<pass>_ms` | GPU time per pass, a few frames late; negative when not available. |
| `cpuscope_<pass>_ms` | CPU recording time of the same passes. |

### 10.3 Scenario scripts

The benchmark level needs a level script that keeps the player alive and moves the camera. Copies are in this folder;
copy them to `Scripts/Levels/` of the game directory and add the levels to `Scripts/Gameflow.lua`:

```lua
bench = Level.new()
bench.nameKey = "level_test"
bench.levelFile = "Data/Benchmark_tut1_water.ten"
bench.scriptFile = "Scripts/Levels/Benchmark.lua"
bench.ambientTrack = "108_A8_Coastal.wav"
Flow.AddLevel(bench)
```

| Script | Scenario |
|---|---|
| `Benchmark.lua` | Turns the player slowly so the chase camera sweeps the scene; emits additive, alpha blended and distortion particles, a shadow casting light, a colored fog bulb and a lens flare, and enables depth of field, so every main pass runs. |
| `Sweep.lua` | Moves the player to every moveable of the level and turns a full circle at each stop, logging each stop; matches slow frames with places in large levels. |
| `InventoryTest.lua` | Opens the inventory, for the menu capture of the regression test. |

Benchmark levels used so far: `Benchmark_tut1_water`, `Benchmark_karnak`, `Benchmark_coastal`, `Benchmark_sky`,
compiled from `.prj2` projects with the TombLib level compiler.

### 10.4 Regression test

`RendererParity.ps1` runs `Benchmark.lua` on every level and backend, compares `Logs/Benchmark.png` with
`Logs/Reference/<level>.png`, and checks the output for Vulkan validation errors (`VUID-`) and device loss:

```
powershell -ExecutionPolicy Bypass -File Documentation\Renderer\RendererParity.ps1 [-BuildDir Build\Debug]
    [-Levels Benchmark_coastal,...] [-Backends vulkan,dx11] [-Threshold 5] [-Seconds 12]
    [-ExtraArgs lowpower] [-ReferenceDir Build\Debug\Logs\Reference]
```

A run passes when no more than `Threshold` percent of the sampled pixels differ by more than 8 in a channel. The
level name `Title` captures the title menu. Results and images go to `Logs/Parity`. The references were taken at 30
fps, so menu animations differ more with high framerate on.

### 10.5 Measuring correctly

- **Use Release shaders.** A Release executable started from `Build/Debug` loads the Debug shaders (compiled with
  `-Od`); point it at a Release build directory or copy `Build/Release/Shaders/Bin`.
- **Uncap the frame rate** with `novsync` when comparing GPU or CPU cost; with vertical sync the frame time is the
  refresh interval.
- **Remote desktop distorts results.** RDP caps presentation through vertical sync; measure locally.
- **Alternate A/B runs.** Vulkan GPU times vary by about 0.3 ms between identical runs; compare builds with
  alternating runs, not single runs.
- **Video memory.** Other applications hold video memory that Windows does not evict, and on Vulkan textures created
  after the level filled the memory can land in system memory. Check the memory line of the log and the F8 panel when
  a backend is unexpectedly slow.
- **Hybrid GPUs.** The renderer picks the high performance GPU; `lowpower` measures the integrated one.
- **GPU timing cost.** On SDL GPU, run once with and once without `nogputiming` to see the cost of the split command
  buffers.

---

## 11. Configuration options affecting the renderer

| Option | Values | Notes |
|---|---|---|
| Renderer | Vulkan, DirectX 11, Direct3D 12, OpenGL | Applied at the next start. |
| Screen resolution, windowed mode | | |
| Render scale | Native, ultra quality, quality, balanced, performance | FSR 1 below native; the material mip bias keeps the texture detail of the window resolution. |
| Antialiasing | None, low (FXAA), medium (SMAA), high (SMAA) | |
| Ambient occlusion | On, off | Half resolution SSAO; also requires the level setting. |
| Shadows | None, player, all | Cube shadow map of one light, map size in the configuration; blob shadows otherwise. |
| Caustics, decals | On, off | |
| High framerate | On, off | Interpolates frames between game ticks up to the refresh rate. |
| Atmospheric sky quality | Low, medium, high | Cost of the atmospheric dome and volumetric clouds. |
| Gamma | | Applied by the post-process. |
