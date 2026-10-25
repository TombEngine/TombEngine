# Renderer Roadmap

This document lists renderer improvements that are planned but not scheduled yet: for each one, the goal, the state
of the code today, a concrete design, the expected gain, the costs and the dependencies. It ends with a recommended
order and a completion criterion per item. It assumes the architecture described in `RendererArchitecture.md` and the
transparent pass described in `TransparentSorting.md`; it supersedes the transparent and GPU-driven parts of
`ModernizationPlan.md` (Italian), which remains the reference for memory and asset goals.

> Implemented and described in `RendererArchitecture.md`: R2 motion vectors and TAA, R3 dynamic resolution, R6 clustered
> lights for objects and sprites, R7 temporal GTAO.

Item prefixes: **S** for the sorted (transparent) faces, **R** for the rest of the renderer. Unless stated otherwise,
paths are relative to `TombEngine/Renderer`.

Facts that several items build on:

- The RHI (`RHI/RHIDevice.h`) has `IComputePass`, `ComputePassDesc` and `CreateComputePipeline`, but no backend code
  records a compute pass yet.
- `IRenderPass` has `Draw` and `DrawIndexed` only: no indirect draws.
- No pass writes motion vectors.
- The scene target and the scene pipelines use `IDevice::GetSwapchainFormat()` (`Backend/SceneRenderer.cpp`), so the
  scene is rendered in LDR.
- Transparent faces are collected, clustered (union-find) and radix sorted on the CPU every frame, and the sorted
  indices are uploaded every frame (`Frame.SortedIndices`).
- Sprites never join a cluster, so they are not interleaved with mesh clusters.
- Light clusters (`Backend/SceneRendererLightClusters.cpp`) are read by room pixels only.

---

## Sorted faces

### S1 - One blend state for alpha blend, premultiplied and additive

**Goal.** Merge consecutive sorted runs of the most common blend modes into one draw without changing the image.

**Current state.** `GetBlendState` (`Backend/SceneRenderer.cpp`) gives `AlphaBlend` (`SrcAlpha, OneMinusSrcAlpha`),
`PremultipliedAlphaBlend` (`One, OneMinusSrcAlpha`) and `Additive` (`SrcAlpha, One`) different states, so
`GetScenePipeline(shader, draw.BlendMode)` returns a different pipeline per mode and `DrawSortedFaces` rebinds it on
every change. `CanMergeSortedRoomRuns` refuses runs whose `BlendMode` differs. The per-draw `BlendMode` is already in
`TEN_DRAW_PIXEL_UNIFORMS` (`Shaders/GpuShared.hlsli`).

**Proposal.**

1. Add a blend family `PremultipliedOver` with state `One, OneMinusSrcAlpha` for color and alpha.
2. In the room, object and sprite pixel shaders, finish the color by the per-draw `BlendMode`:
   alpha blend outputs `(rgb * a, a)`, premultiplied outputs `(rgb, a)`, additive outputs `(rgb * a, 0)`.
3. Map the three modes to the same pipeline in `GetScenePipeline` for the sorted pass only; the unsorted additive
   pass keeps its current pipeline until measured.
4. Relax `CanMergeSortedRoomRuns` (and add the same test for object runs of one instance) to compare the blend family
   instead of the blend mode, with the per-draw `BlendMode` uniform moved into per-vertex or per-instance data so a
   merged run can mix modes (rooms: a bucket flag in the vertex; objects: the instance).
5. Subtractive, screen, lighten and exclude keep their own pipelines and runs.

**Expected gain.** Fewer pipeline binds and draws in scenes that mix glass, smoke and additive effects; no visual
change (identical blend equations, up to rounding of the 8-bit target).

**Cost / risks.** Every shader writing to the sorted pass must apply the conversion, including mirrored and inventory
variants; a missed one shows as too bright or dark faces. The merge in step 4 needs the blend mode out of the draw
uniforms.

**Dependencies.** None.

### S2 - Temporal coherence in sorting

**Goal.** Make the sort cost proportional to how much the order changes, not to the face count.

**Current state.** `SortTransparentFaces` (`Scene/RendererSorted.cpp`) builds keys from scratch and runs `RadixSortKeys`
(LSD, 8-bit digits, usually three passes) every frame. `CollectSortedBucket` transforms every polygon centre with the
world matrix, including rooms (identity) and statics, whose centres never move.

**Proposal.**

1. Keep the previous frame's sorted keys, identified by a stable id (group key plus polygon index).
2. When the camera moved less than a threshold and the set of groups is the same, rebuild only the distances, put the
   keys in the previous order and fix it with an insertion sort, which is O(n) on nearly sorted data.
3. Fall back to the full radix sort on camera cuts, teleports, flyby jumps, a changed group set, or when the insertion
   sort exceeds a move budget (abort and redo).
4. At load, store world-space polygon centres for room and static buckets (statics: once per static instance in
   `PrepareDataForTheRenderer`), so collection skips the transform for them.

**Expected gain.** Sort time close to one linear pass in normal gameplay; less collection time on levels with many
transparent room and static faces.

**Cost / risks.** Extra memory for the previous order and the per-instance static centres. Ties must resolve as the
radix sort does (stable, by cluster rank), or the order flickers between the two paths.

**Dependencies.** None. Combines with S8.

### S3 - Convex closed transparent meshes without per-polygon sort

**Goal.** Draw closed convex transparent meshes (bottles, vases, glass boxes) correctly without sorting their polygons.

**Current state.** Every polygon of a sorted bucket becomes a `RendererSortablePolygon` and a sort key; a run is built
per contiguous group in `SceneRenderer::CollectSortedFaces`, and its indices are copied into `_sortedIndexData`.

**Proposal.**

1. At load, mark meshes whose transparent buckets form a closed (every edge shared by two faces) convex surface.
2. Collect such objects as a single sortable entry (like a sprite), placed by its centre.
3. Draw them in two passes with culling: back faces, then front faces, both from the static level index buffer, so
   no sorted indices are copied.
4. Objects that overlap another transparent object still join the cluster logic as whole objects.

**Expected gain.** Fewer sort keys and no index copy for the most common transparent props; per-polygon order inside
a convex mesh is exact by construction.

**Cost / risks.** Detection must be conservative (tolerance on convexity, welded vertices). Skinned meshes are
excluded. Two pipelines (front and back culling) per affected blend mode.

**Dependencies.** None.

### S4 - Sprites inside clusters

**Goal.** Fix particles seen through glass and glass seen through particles.

**Current state.** `CanInterleaveObject` (`Scene/RendererSorted.cpp`) keeps rooms and sprites out of
`BuildSortedClusters`; sprite groups (texture, 4-block depth band, blend mode) are ordered as whole groups. In
`DrawSortedFaces`, a sprite run resets the pipeline and bind state (`DrawSpriteRun`).

**Proposal.**

1. Give sprite groups depth ranges and screen rectangles (already computable from the sprite positions and size) and
   let them join clusters.
2. Inside a cluster, sprites sort by their own distance among mesh polygons; the sprite group still breaks into runs
   only where the order requires it.
3. Bound the cost with the existing `SplitBusyClusters` limit, counting sprite run switches too.
4. Restore the mesh bind state after a sprite run without a full reset (track the sampler and uniform slots the sprite
   run overwrote).

**Expected gain.** Correct ordering of smoke, fire and dust against glass and water-like transparent meshes.

**Cost / risks.** More runs in clusters that mix sprites and meshes; the switch between the sprite and mesh paths
costs a pipeline bind. Becomes cheaper after S1, which lets sprites and meshes share the blend state.

**Dependencies.** S1 recommended. Interacts with S5 and S6: particles moved to a reduced-resolution target or to OIT
leave the clusters again.

### S5 - Half and quarter resolution particles

**Goal.** Cut the fill rate of large overlapping particles (smoke, fire, dust), the main GPU cost of particle-heavy
scenes on integrated GPUs.

**Current state.** All sprites draw at render resolution in the scene pass; soft particles read the depth copy
(`IsSoftParticle` on the sprite run).

**Proposal.**

1. Tag sprite types that tolerate low resolution (smoke, fire, dust, mist) in the frontend.
2. Draw them, unsorted against meshes but sorted among themselves, into a half or quarter resolution RGBA16F target
   with a downsampled depth (min or max depth per 2x2 block) for the soft particle fade.
3. Composite into the scene target with a depth-aware upscale: bilateral weights from the full and low resolution
   depths, or nearest-depth sampling at edges.
4. Quality option: full, half, quarter.

**Expected gain.** Up to 4x (half) or 16x (quarter) fewer particle pixels shaded and blended.

**Cost / risks.** Halo artifacts at depth edges if the upscale is too simple; particles in front of transparent
geometry lose correct ordering with it (composited after the sorted pass or before, a fixed choice).

**Dependencies.** None. Benefits from R1 (HDR target for accumulation).

### S6 - Weighted blended OIT for particles (option)

**Goal.** Remove sorting cost and popping for dense particle systems.

**Current state.** Particles with sorted blend modes go through the frontend sort; group order inside a depth band is
approximate.

**Proposal.**

1. Opt-in per sprite type or per quality preset.
2. Draw selected particles without sorting into an accumulation target (RGBA16F, additive) and a revealage target
   (R8 or R16F, multiplicative), with the depth weight of McGuire and Bavoil.
3. Composite once over the scene. Geometry keeps the sort.

**Expected gain.** No CPU sort or per-frame index work for those particles, no order popping.

**Cost / risks.** Approximate result: different look for strongly opaque particles; two extra targets. Interacts with
S5 (both can share the reduced-resolution target).

**Dependencies.** None; S5 recommended first.

### S7 - GPU sort (Vulkan and Direct3D 12)

**Goal.** Move key building, sorting and index generation to the GPU and drop the per-frame index upload.

**Current state.** Keys are built and sorted on the CPU (`SortTransparentFaces`), runs are built and indices copied on
the CPU (`SceneRenderer::CollectSortedFaces`), then uploaded with `UploadFrameStorage(_sortedIndexBuffer, ...)`.

**Proposal.**

1. The CPU keeps object order and clusters (union-find) and writes one record per sortable object: base polygon,
   count, instance, cluster rank, run slot.
2. A compute pass computes keys from polygon centres (precomputed at load, see S2) and the instance transforms.
3. A subgroup radix sort (reduce-then-scan, which does not depend on forward progress guarantees as onesweep does)
   sorts keys per frame.
4. A compute pass writes sorted indices straight into the index buffer and the indirect draw arguments of each run;
   unused runs get zero indices. Pipelines per run are known to the CPU from the cluster sequence, with a fixed maximum
   of runs per cluster.
5. DirectX 11 and OpenGL keep the CPU path.

**Expected gain.** No sorted index upload and no CPU sort on levels with many transparent faces; CPU cost of the
transparent pass close to constant.

**Cost / risks.** Shader Model 6 subgroup operations on the Vulkan and Direct3D 12 path only; a second implementation
to keep in sync with the CPU ordering rules; debugging is harder.

**Dependencies.** R4 (indirect draws in the RHI), first use of compute passes.

### S8 - Parallel collection

**Goal.** Spread transparent face collection over the worker threads.

**Current state.** `Renderer::CollectSortedFaces` (`Scene/RendererDraw.cpp`) calls the `CollectSorted*Faces` functions
in sequence on the game thread; each appends to `view.TransparentObjectsToDraw` and `TransparentPolygonsToDraw`.

**Proposal.**

1. Split the work by room (room buckets, statics and items of the room) and by effect list.
2. Run the tasks with `g_Parallel.AddTasks`, each writing to its own object and polygon lists.
3. Merge the lists in room order and fix `ObjectIndex` offsets, so the result is identical to the serial order.
4. Keep the serial path below a polygon count threshold, where the task overhead would dominate.

**Expected gain.** Collection time divided by the number of cores on large transparent scenes.

**Cost / risks.** Shared state in collection (hair instances, swarm interpolation) must be read-only during the tasks;
the merge costs one copy.

**Dependencies.** None. Combines with S2.

---

## Rest of the renderer

### R1 - HDR pipeline

**Goal.** Render the scene in a floating point target, tonemap it, and derive bloom from the HDR image.

**Current state.** The scene target and the scene pipelines use `GetSwapchainFormat()` (`Backend/SceneRenderer.cpp`).
Glow (`Backend/SceneRendererGlow.cpp`) blurs only the emissive target written by the prepass, so bright lights and
additive effects do not bloom.

**Proposal.**

1. Scene target in RGBA16F (R11G11B10F when alpha is not needed and memory is short); scene, sky and water pipelines
   created for that format.
2. Tonemapping at the start of the post-process, before color scheme, gamma and AA; exposure fixed or from a level
   setting first, automatic later.
3. Bloom from a bright pass of the HDR image (downsample chain, upsample with a tent filter), replacing the
   emissive-only glow; the emissive target stays for the prepass if other passes need it.
4. A "classic" preset (identity-like tonemap, no bloom) that keeps today's image within the parity threshold of
   `RendererParity.ps1`.

**Expected gain.** No clipping of strong lights and additive stacks; physically plausible bloom; base for TAA
resolve and S5/S6 accumulation.

**Cost / risks.** Twice the bandwidth of the scene target; every blend mode must behave the same in float (subtractive
and exclude can go negative and need a clamp); look changes outside the classic preset.

**Dependencies.** None. Interacts with R2 (TAA resolve before tonemapping).

### R4 - Indirect draws and GPU culling

**Goal.** Cull and submit room and object geometry on the GPU, with draw counts independent of the level size.

**Current state.** `IRenderPass` has no indirect draw. Rooms are culled per chunk on the CPU against the portal
rectangles; the texture array layer is a per-draw uniform (`TextureLayer` in `TEN_DRAW_PIXEL_UNIFORMS`), so draws of
one texture array group with different layers cannot merge.

**Proposal.**

1. RHI: `DrawIndirect` and `DrawIndexedIndirect` with a draw count (SDL GPU `SDL_DrawGPUIndexedPrimitivesIndirect`,
   DirectX 11 a loop of `DrawIndexedInstancedIndirect`, OpenGL `glMultiDrawElementsIndirect`). Culled draws have zero
   instances.
2. Move the texture layer (and material index) into the material table read by the shaders, so one indirect call
   covers a pipeline and texture group.
3. Compute culling per chunk: frustum and portal rectangle of the room, then two-phase Hi-Z occlusion from the
   previous frame's depth (draw last frame's visible set, build Hi-Z, test the rest).
4. DirectX 11 keeps CPU culling with merged index ranges, or uses the same compute where measured faster.

**Expected gain.** Constant draw calls per pipeline on large levels; occlusion of geometry inside visible rooms that
portals do not remove.

**Cost / risks.** First use of compute and storage buffer writes in the RHI on every backend; Hi-Z needs a depth
pyramid pass; harder debugging of missing geometry.

**Dependencies.** None for step 1; steps 3 and 4 after step 2. Required by S7.

### R5 - Cached static shadow maps

**Goal.** Let rooms and statics cast shadows at low cost, then support several shadow lights.

**Current state.** `RenderShadowMap` (`Backend/SceneRendererShadows.cpp`) renders six cube faces of one light every
frame with the draws copied by `MarkShadowCasterDraws`, which are moveables and the player only (`IsShadowCaster`);
rooms and statics do not cast.

**Proposal.**

1. Static cube map per light, with room and static casters, rendered once when the light first becomes the shadow
   light and invalidated when it moves or its range changes.
2. Each frame, copy the static map into the frame map and render only moving casters on top (or sample both maps and
   take the minimum).
3. Several shadow lights in an atlas of cube faces, sized by screen coverage and distance, with a budget of
   re-rendered faces per frame.

**Expected gain.** Room and static shadows at the cost of moving casters only.

**Cost / risks.** Memory per cached light; a look change (rooms now cast), so a setting or level option keeps the
current behaviour.

**Dependencies.** None. R4 culling can feed the cube faces later.

### R8 - Lightweight frame graph

**Goal.** Simplify pass code and reduce render target memory.

**Current state.** `FRAME_PASSES` lists the passes in a fixed order with flags; intermediate targets come from
`RHI::RenderTargetPool` and are reserved up front; each pass binds and transitions its own resources.

**Proposal.**

1. Passes declare the textures and buffers they read and write, and the transient targets they create.
2. A compile step culls passes whose outputs are unused, computes lifetimes and aliases transient targets with
   compatible descriptions in the pool.
3. Execution stays single-threaded and in declaration order; no automatic reordering.

**Expected gain.** Less render target memory (DOF, glow, SSAO, reflections and distortion targets rarely live at the
same time); fewer pass flags and manual resource handling.

**Cost / risks.** A rewrite of `RenderFrame` pass by pass; aliasing on SDL GPU must respect cycling and the frames in
flight.

**Dependencies.** None. Useful before adding the targets of R1, S5 and S6.

### R9 - Render thread

**Goal.** Overlap the frontend of frame N+1 with the backend recording of frame N.

**Current state.** The game thread runs the game logic, the frontend (`Renderer::RenderScene`) and the backend
(`SceneRenderer::RenderFrame` from `SubmitFrame`, then `FinishFrame`) in sequence. OpenGL keeps one context current on
one thread; `SceneRenderer::ReleaseThread()` hands it over at game thread start and exit and around the asynchronous
level load.

**Proposal.**

1. The frontend fills a `RenderView`, `FrameEnvironment` and `UiFrame` that become immutable once submitted
   (double-buffered).
2. A render thread runs `RenderFrame` and `FinishFrame` from the submitted copies and owns the device; on OpenGL the
   context moves to it with `ReleaseThread` at startup and stays there.
3. Pointers into frontend data (`RendererSortableObject::Bucket`, items, rooms) must stay valid for one frame: either
   copy the needed values or delay level unloads by one frame.

**Expected gain.** Frame time close to max(frontend, backend) instead of their sum when CPU bound.

**Cost / risks.** One frame of extra latency; all level load and unload paths must synchronize with the render thread;
data races in shared renderer state.

**Dependencies.** S8 and R11 first, to know where the CPU time is.

### R10 - Assets with Tomb Editor

**Goal.** Move load-time geometry processing to the level compiler and ship GPU-ready meshes.

**Current state.** `SceneRendererWater.cpp` builds water surfaces at load (`LoadWaterSurfaces`, `AddWaterSurface`,
`HideWaterSurfaceBuckets`), temporarily, as documented in `RendererArchitecture.md`. Vertices are the full
`RendererVertex` layout, not optimized for the vertex cache; meshes have no LODs.

**Proposal.**

1. Tomb Editor computes the water surface grids and the hidden fake surface faces and writes them to the level; the
   engine only draws them, with the current code kept as a fallback for older levels.
2. Quantized vertices (16-bit positions relative to the mesh bounds, packed normals and UVs) and vertex cache and
   overdraw optimized indices, produced with meshoptimizer in Tomb Editor.
3. Two to four LODs for statics and moveables, chosen by screen size, with a dithered transition; disabled at high
   settings.

**Expected gain.** Shorter loads, less vertex memory and bandwidth, fewer triangles for distant objects.

**Cost / risks.** Level format change coordinated with Tomb Editor; conversion path for old levels; new dependency
(meshoptimizer, MIT) in Tomb Editor.

**Dependencies.** Format decision with Tomb Editor.

### R11 - Tracy profiler zones

**Goal.** Measure CPU time per frontend and backend step with low overhead.

**Current state.** The benchmark CSV (`Specific/Benchmark.cpp`) records whole-frame CPU times, the sorted collect and
sort times and `cpuscope_<pass>_ms` per backend pass; frontend steps (culling, collection, sprites, sky, HUD) have no
individual timing.

**Proposal.**

1. Add Tracy as an optional dependency, compiled out unless enabled.
2. Zones on the frontend steps of `RenderScene`, on `SubmitFrame`, on the backend collection, upload and recording, and
   on worker tasks (S8).
3. Export the main zones as columns of the benchmark CSV, from a lightweight internal timer that Tracy also wraps, so
   the CSV does not require Tracy.

**Expected gain.** Data to prioritize S2, S8 and R9; regressions visible per step.

**Cost / risks.** Small; zone overhead when enabled.

**Dependencies.** None.

### R12 - FSR 3.1 temporal upscaler

**Goal.** Replace TAA and the FSR 1 upscale below native scale with one temporal upscaler of better quality, so lower
render scales (down to 50%) stay sharp on integrated GPUs.

**Current state.** TAA (`Backend/SceneRendererTAA.cpp`) resolves at the render resolution; below native scale FSR 1
(EASU and RCAS) upscales its result. A temporal upscale inside the TAA resolve, with the history at the window
resolution, was measured and removed: at 67% it cost 2.5 ms more than FSR 1 on the integrated GPU (17.3 against
14.5 ms per frame on DirectX 11), for a modest quality gain.

**Proposal.**

1. Port the FidelityFX FSR 3.1 upscaler passes (HLSL compute, MIT) to RHI compute pipelines, so they run on every
   backend; the FidelityFX SDK backends need native Vulkan handles that SDL GPU does not expose, and have no DirectX 11
   or OpenGL backend.
2. Feed it the prepass depth, motion vectors and jitter of the current TAA path.
3. Write a reactive mask from the sorted pass and the sprites (alpha blended faces, particles, scrolling and animated
   textures, caustics), which have no correct motion vectors.
4. Offer it as the temporal antialiasing setting: at native scale it antialiases, below it upscales; SMAA and no
   antialiasing keep FSR 1.

**Expected gain.** Clearly better image than FSR 1 at 67% and 50%, so the automatic scale can drop further for the same
look.

**Cost / risks.** Around ten compute passes to port and keep in sync with upstream; ghosting on effects the reactive
mask misses; its cost at window resolution on integrated GPUs must be measured against TAA plus FSR 1.

**Dependencies.** None. Interacts with R1 (resolve before tonemapping).

---

## Recommended order

| # | Item | Depends on | Done when |
|---|---|---|---|
| 1 | S1 One blend state | - | Alpha blend, premultiplied and additive sorted runs share one pipeline and merge; parity test passes on every backend. |
| 2 | S2 Temporal coherence | - | Sort uses the incremental path in normal gameplay and the radix sort only on cuts; same image; sort time reported lower in the CSV. |
| 3 | S8 Parallel collection | - | Collection runs on `g_Parallel` above the threshold with output identical to the serial path; collect time lower on large scenes. |
| 4 | R11 Tracy zones | - | Frontend and backend zones visible in Tracy; main zones exported as CSV columns. |
| 5 | S3 Convex meshes | - | Closed convex transparent meshes draw as two passes without sort keys; no visual difference on the benchmark levels. |
| 6 | S5 Reduced resolution particles | - | Smoke, fire and dust draw at half or quarter resolution with a depth-aware composite and no visible halos; particle GPU time lower. |
| 7 | S4 Sprites in clusters | S1 | Particles behind and in front of glass sort correctly; draw count within the cluster switch limit. |
| 8 | R1 HDR | - | Scene in RGBA16F or R11G11B10F with tonemapping and HDR bloom; classic preset passes the parity test. |
| 9 | R4 Indirect draws and GPU culling | - | Indirect draws on every backend; GPU culling with Hi-Z on Vulkan and Direct3D 12; constant draws per pipeline on large levels. |
| 10 | S7 GPU sort | R4 | Sorted faces sorted and indexed on the GPU on Vulkan and Direct3D 12; no per-frame sorted index upload. |
| 11 | R5 Cached shadow maps | - | Rooms and statics cast from a cached map; per-frame shadow cost limited to moving casters. |
| 12 | R8 Frame graph | - | Passes declare resources; transient targets alias; render target memory lower in the log. |
| 13 | R9 Render thread | S8, R11 | Frontend and backend overlap on every backend, OpenGL included; no frame data shared between the threads without a copy or a fence. |
| 14 | S6 Weighted blended OIT | S5 | Option for selected particles with accumulation and revealage targets; measured against S5 on the benchmark levels. |
| 15 | R10 Tomb Editor assets | Format decision | Water surfaces, quantized optimized vertices and LODs come from the level file; old levels still load. |
| 16 | R12 FSR 3.1 upscaler | - | FSR 3.1 replaces TAA on every backend, with a reactive mask for sorted faces and sprites; better than TAA plus FSR 1 at 67% on the benchmark levels at a measured cost. |

S1, S2, S8 and R11 are cheap and do not change the look: they go first. S3, S5 and S4 then improve the transparent
pass and particles. R1 is the main visual change and precedes the targets of S5 and S6 where possible. R4 opens the
GPU-driven path and S7 builds on it. S6 is an option to evaluate after S5. R5, R8, R9 and R10 follow as needed.
