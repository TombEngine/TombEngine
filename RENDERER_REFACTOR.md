# Renderer Refactor — Pipeline State, Render Passes, Debug Regions

## Why

The renderer needed to be made Vulkan-ready without breaking the existing DX11
backend. Vulkan demands three things that DX11 lets you fudge:

1. **Explicit render passes** — every render-target switch is an explicit
   `BeginRenderPass` / `EndRenderPass` with declared attachments and load/store
   actions.
2. **Pre-baked pipeline state** — blend/depth/cull/shader/format are bundled
   into a single immutable PSO that's bound atomically.
3. **Named GPU regions** — required for any non-trivial profiling/capture in
   tools like RenderDoc, PIX, NSight.

The refactor introduces a thin abstraction over these concepts that compiles
to the existing DX11 calls today, but maps 1:1 to Vulkan when a Vulkan
backend is added.

## What was added

### `RenderPipelineState` — `Renderer/Graphics/RenderPipelineState.h`

A POD triplet `{ BlendMode Blend; DepthState Depth; CullMode Cull; }` with
equality, a hash functor, and a set of `constexpr` presets in the
`Pipelines::*` namespace covering the combinations the renderer actually uses:

| Preset                       | Blend       | Depth | Cull              |
|------------------------------|-------------|-------|-------------------|
| `Pipelines::OpaqueDefault`   | Opaque      | Write | CounterClockwise  |
| `Pipelines::AlphaTestDefault`| AlphaTest   | Write | CounterClockwise  |
| `Pipelines::Additive`        | Additive    | Read  | CounterClockwise  |
| `Pipelines::AdditiveNoCull`  | Additive    | Read  | None              |
| `Pipelines::AlphaBlend`      | AlphaBlend  | Read  | CounterClockwise  |
| `Pipelines::FullscreenPass`  | Opaque      | Write | CounterClockwise  |
| `Pipelines::DebugLines`      | Additive    | Read  | None              |
| `Pipelines::HudNoDepth`      | Opaque      | None  | None              |
| `Pipelines::Lines2D`         | Opaque      | Read  | None              |
| `Pipelines::OpaqueNoCull`    | Opaque      | Write | None              |
| `Pipelines::OpaqueCW`        | Opaque      | Write | Clockwise         |

The hash makes presets directly usable as keys in a future PSO cache.

### `RenderPassDescriptor` — `Renderer/Graphics/RenderPassDescriptor.h`

Describes a render pass: any number of color attachments, one optional depth
attachment, viewport, and a name (used as the GPU debug-region label). Each
attachment carries a `LoadAction` (`Load` / `Clear` / `DontCare`) and
`StoreAction` (`Store` / `DontCare`) — the same semantics Vulkan and Metal
require. `ColorAttachmentDescriptor::ClearColor` uses `XMVECTORF32` to match
the existing `ClearRenderTarget2D` signature.

### `IGraphicsDevice` virtual API

Four new virtual methods, all with default implementations that decompose to
existing calls so the DX11 backend keeps working without changes:

```cpp
virtual void BeginRenderPass(const RenderPassDescriptor& desc);
virtual void EndRenderPass();
virtual void BeginDebugEvent(const std::string& name);
virtual void EndDebugEvent();
```

The default `BeginRenderPass` walks the descriptor and calls
`ClearRenderTarget2D` / `ClearDepthStencil` / `BindRenderTarget(s)` /
`SetViewport` / `SetScissor` for each attachment as needed. `EndRenderPass`
is a no-op by default.

The DX11 backend overrides only the debug events, querying
`ID3DUserDefinedAnnotation` once after `D3D11CreateDevice` and routing
`BeginDebugEvent` / `EndDebugEvent` through it. Best-effort: if the runtime
doesn't expose the interface, the calls become no-ops.

### `Renderer` wrappers

```cpp
void BindPipeline(const RenderPipelineState& state, bool force = false);
void BeginRenderPass(const RenderPassDescriptor& desc);
void EndRenderPass();
```

`BindPipeline` sets blend / depth / cull in one call with the same
`_lastBlendMode` / `_lastDepthState` / `_lastCullMode` caching used by the
individual setters. Crucially it inlines the blend update so it avoids
`SetBlendMode`'s implicit `SetDepthState(Write|Read)` side-effect — the
explicit `Depth` from the pipeline preset always wins.

`BeginRenderPass` / `EndRenderPass` forward to the device and bracket the
pass with `BeginDebugEvent(desc.Name)` / `EndDebugEvent()` automatically, so
any pass authored with this API gets a named region in captures for free.

## How to use

### Open and close a render pass

```cpp
RenderPassDescriptor pass;
pass.Name = "My Pass";

ColorAttachmentDescriptor color;
color.RenderTarget = _myRT->GetRenderTarget();
color.LoadAction   = LoadAction::Clear;
color.ClearColor   = Colors::Black;
pass.ColorAttachments.push_back(color);

pass.DepthAttachment.DepthTarget = _myRT->GetDepthTarget();
pass.DepthAttachment.LoadAction  = LoadAction::Clear;
pass.DepthAttachment.ClearFlags  = DepthStencilClearFlags::DepthAndStencil;
pass.Viewport = view.Viewport;

BeginRenderPass(pass);
// ... draws into _myRT
EndRenderPass();
```

For a multi-RT (MRT) pass, push more `ColorAttachmentDescriptor`s. For an
"open scene → write more on top" pattern, use `LoadAction::Load` to keep the
previous contents.

### Bind a pipeline preset

```cpp
BindPipeline(Pipelines::OpaqueDefault);
// subsequent draws use Opaque blend, Write depth, CCW culling
```

`force = true` reapplies the state even when the cache says it's already
current — useful when an external sub-system (post-process effect, sprite
batch) may have changed device state under the renderer's nose.

### Annotate a non-pass code section

```cpp
_graphicsDevice->BeginDebugEvent("My Section");
// ... existing render code, no other changes
_graphicsDevice->EndDebugEvent();
```

Useful for sections that manage their own RT bindings internally (post-process
chain, antialiasing, glow) — wrapping just gives RenderDoc a named region.

## What's converted today

`RenderScene` is now a sequence of explicit passes:

- **Sky** — `BeginRenderPass(skyPass)` clears main color+depth, draws horizon
  and sky, `EndRenderPass`.
- **GBuffer** — three MRT (normals, depth, emissive/roughness), shares the
  main depth via `LoadAction::Load` to keep sky depth.
- **Opaque + Transparent** — main RT with `LoadAction::Load` on color (sky)
  and depth (GBuffer); runs all `RendererPass::Opaque/Additive/Transparent/
  GunFlashes` plus `DrawLines3D` / `DrawTriangles3D`.
- **HUD 3D** — main RT color `Load`, depth `Clear` so HUD geometry sits on
  top of the scene.

Sections that manage their own RT bindings get debug-event wrappers:
`Sky for Reflections`, `SSAO`, `Reflections Copy`, `Glow`, `Antialiasing`,
`HUD 2D`, `Postprocess`, `Overlays`, `Display Sprites + Strings`.

`BindPipeline` is in use at:

- `SetupBlendModeAndAlphaTest` — the centralized hot path called by the room,
  item, static, and moveable draw loops (~10 call sites). Maps each
  `RendererPass + bucket.BlendMode` combination to the right preset.
- `ApplySMAA`, `ApplyFXAA`, `ApplyGlow`, `CopyRenderTarget`, `DrawPostprocess`
  setup → `Pipelines::FullscreenPass`.
- `DrawBar`, `DrawLoadingBar` → `Pipelines::HudNoDepth`.
- `DrawLines2D` → `Pipelines::Lines2D`.
- `RenderInventoryScene`, `RenderTitle` reset, `DrawObjectIn3DSpace`
  → `Pipelines::OpaqueDefault`.
- `RenderScene` initial state reset → `Pipelines::OpaqueDefault`.

## What's still using the old per-call API

These are intentional — converting them in this round would have inflated
the diff without buying anything for Vulkan readiness:

- Per-bucket draw paths in `RendererDrawMenu.cpp` that compute blend mode
  from `GetBlendModeFromAlpha(bucket.BlendMode, color.w)`. The blend depends
  on per-draw alpha, not a static preset, so the runtime
  `RenderPipelineState` is built dynamically (or `SetBlendMode` is kept).
- A few partial duets (`SetBlendMode + SetCullMode` without `SetDepthState`)
  in menu and title flows — converting them would force a depth state where
  none was intended.
- `DrawHorizonAndSky` paraboloid mode — has explicit
  `SetCullMode(CullMode::Clockwise)` for the second hemisphere, mapped to
  `Pipelines::OpaqueCW` in a follow-up if/when the paraboloid path is
  revived.

## Backend impact

DX11 needs no changes beyond the new debug-event override — the default
`BeginRenderPass` decomposes into the same `Clear*` / `Bind*` / `SetViewport`
calls the renderer was making before. A future Vulkan or Metal backend can
override `BeginRenderPass` natively (`vkCmdBeginRenderPass`,
`MTLRenderCommandEncoder`) without any caller-side changes.

A future PSO cache lives entirely inside `IGraphicsDevice::BindPipeline`:
hash the `RenderPipelineState`, look up the cached PSO, create-on-miss. The
caller's contract is unchanged.

## Test gate

The conversion preserves visual output 1:1. Verification:

1. Build DX11 and load a level.
2. Capture a frame in RenderDoc / PIX. Expect a labelled timeline in this
   order: `Sky for Reflections` → `Sky` → `GBuffer` → (`SSAO`) →
   `Opaque + Transparent` → `Reflections Copy` → `HUD 3D` → `Glow` →
   `Antialiasing` → (`HUD 2D`) → `Postprocess` → `Overlays` →
   `Display Sprites + Strings`.
3. Animated textures, transparency, particle effects, HUD bars, inventory
   3D items, and 2D overlays should render identically to before.
