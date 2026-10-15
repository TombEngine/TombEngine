# Renderer architecture

The renderer is split into a frontend, which decides what is drawn, and a backend, which draws it on the GPU through a small rendering hardware interface (RHI) with DirectX 11, SDL GPU (Vulkan, Direct3D 12) and OpenGL 4.5 implementations.

## Folders

| Folder | Content |
|---|---|
| `Renderer/` | `Renderer` class (`g_Renderer`), the interface of the game code: initialization, settings, enums and constants. |
| `Renderer/Scene/` | Frontend of the 3D scene: room and portal culling, items, statics, lights, effects, sprites, sorted faces, Lara, animation, level data conversion, `RenderView` and `Frustum`. |
| `Renderer/UI/` | 2D layer: strings, fonts, HUD bars, menus and inventory, debug overlay and ImGui windows. |
| `Renderer/Sky/` | Sky effects prepared by the frontend: atmospheric dome, sun, moon, aurora, underwater sky, volumetric clouds, god rays and dust storm, with their settings and quality levels. |
| `Renderer/Structures/` | Data of the frontend: rooms, buckets, meshes, objects, items, lights, sprites and the level vertex. |
| `Renderer/ConstantBuffers/` | Uniform blocks filled by the frontend and copied by the backend: camera, lights, fog bulbs, shadows and sky effects. |
| `Renderer/Backend/` | `SceneRenderer` class (`g_SceneRenderer`): GPU resources, pipelines and passes of a frame, one file per area. |
| `Renderer/RHI/` | Device interface (`RHIDevice.h`, `RHITypes.h`), the `DirectX11/`, `SDLGPU/` and `OpenGL/` devices, and shared helpers: shader loading, upload ring, render target pool, readback, memory tracking. `Test/` holds a standalone device test. |
| `Shaders/` | HLSL sources of every shader and `CompileShaders.ps1`, which compiles them offline. |

File names follow the class they implement: `Renderer<Topic>.cpp` files hold methods of `Renderer`, `SceneRenderer<Topic>.cpp` files hold methods of `SceneRenderer`.

## Frame

1. The game calls `g_Renderer.Render()`. The frontend culls rooms through portals, collects items, statics, lights, effects and sprites into a `RenderView`, sorts the transparent faces and prepares the sky effects.
2. `Renderer::SubmitFrame()` fills a `FrameEnvironment` with the values that are not part of the view (fog, post-process, depth of field, sky draws, debug primitives) and calls `g_SceneRenderer.RenderFrame()`.
3. The backend records the passes listed in `SceneRenderer::FRAME_PASSES`: shadow map, depth and normals prepass, depth copy, SSAO, sky reflections, distortion, sky, scene (opaque rooms and objects, then sorted faces), depth of field, reflection source copy, glow, post-process with antialiasing, and upscale. Passes without work in the frame are skipped.
4. The frontend records the 2D layer into a `UiFrame`, which `SceneRenderer::FinishFrame()` composes over the scene.
5. DirectX 11 presents on the game thread. SDL GPU presents on the window thread (`PresentOnWindowThread()`), and frame fences keep the game thread at most two frames ahead of the GPU.

## Level loading

`Renderer::PrepareDataForTheRenderer()` (`Scene/RendererCompatibility.cpp`) converts the level into rooms, buckets, meshes and vertex arrays. `SceneRenderer::LoadLevel()` uploads them into shared vertex and index buffers, and the texture pages into texture arrays grouped by size and format (`SceneRendererTextures.cpp`). When the textures do not fit the free video memory of a discrete GPU, their top mips are left out.

## Shaders

Each `.hlsl` file declares its entry points and resource counts in `// TEN_SHADER:` lines. The build runs `CompileShaders.ps1`, which writes DXBC for DirectX 11, SPIR-V for Vulkan, DXIL for Direct3D 12 and GLSL for OpenGL into `Build/<Configuration>/Shaders/Bin`. The GLSL is translated from the SPIR-V by SPIRV-Cross; the OpenGL device loads each stage as a separate program and offsets its bindings per stage. OpenGL keeps the conventions of the other backends with an upper left clip origin and a depth range from 0 to 1, and flips the image only when it is copied to the window. Its context is current on one thread at a time, so a thread that stops rendering calls `SceneRenderer::ReleaseThread()`. A `permutations=` key adds one variant per define; an `options=` key adds a variant for every combination of its defines. Rooms and objects use options to compile out the material maps a texture group lacks, so the variant is chosen per draw without branches in the shader. `SceneRenderer::ReloadShaders()` recompiles the sources of a development checkout at runtime.

Structures shared with C++ are declared once in `Shaders/GpuShared.hlsli`, which `Backend/GpuTypes.h` includes.

## Further reading

- `TransparentSorting.md`: collection and sorting of the transparent faces.
- `ModernizationPlan.md`: planned work on performance, memory and assets.
