# New Renderer Guide

Quick reference for the Pipeline State Object + Render Pass architecture introduced
across Phase A/B/C. Read this **before** touching renderer code.

---

## 1. Mental model

The renderer is now organised around three concepts that mirror Vulkan / Metal /
SDL_GPU:

| Concept                | What it is                                            | Where                                       |
| ---------------------- | ----------------------------------------------------- | ------------------------------------------- |
| `RenderPipelineState`  | Fixed-function state baked into a "PSO" (blend, depth, cull, topology, input layout, shader, alpha test, sample count). | `Renderer/Graphics/RenderPipelineState.h`  |
| `RenderPassDescriptor` | Declarative render pass: attachments, load/store actions, viewport, scissor, debug label. | `Renderer/Graphics/RenderPassDescriptor.h` |
| Wrapper API on `Renderer` | The only entry point client code should use. Tracks caches, debug events, pass state. | `Renderer/Renderer.{h,cpp}`                |

Everything else (DX11/Vulkan/Metal device, the backend lazy PSO cache, etc.) is
implementation detail.

---

## 2. The Golden Rule

> **Never call `_graphicsDevice->Xxx()` directly from rendering code.**
> Always go through the `Renderer::Xxx()` wrapper.

The wrappers maintain CPU-side dedup caches and debug state. Bypassing them
leaves the engine convinced something is still bound when DX11 has un-bound it,
which produces invisible black screens, missing geometry, or TDR crashes.

The only exceptions are creation methods (`CreateRenderSurface2D`,
`CreateConstantBuffer`, etc.) — those have no cache and are device-only by design.

---

## 3. Defining a render pass

The factory helpers in `RenderPassDescriptor.h` keep call sites readable:

```cpp
// Single-target pass that clears color + depth
{
    RenderPassDescriptor pass;
    pass.ColorAttachments = { ColorAttachmentDescriptor::Clear(_renderTarget->GetRenderTarget(), Colors::Black) };
    pass.DepthAttachment  = DepthAttachmentDescriptor::Clear(_renderTarget->GetDepthTarget());
    pass.HasViewport      = true;
    pass.Viewport         = view.Viewport;
    pass.DebugLabel       = "Main Scene";
    BeginRenderPass(pass);
}

// draw calls...

EndRenderPass();
```

### Helpers

| Factory                                       | Load action       | Use case                                     |
| --------------------------------------------- | ----------------- | -------------------------------------------- |
| `ColorAttachmentDescriptor::Clear(rt, color)` | `Clear`           | First write to this RT in the frame.         |
| `ColorAttachmentDescriptor::Keep(rt)`         | `Load`            | Preserve existing contents.                  |
| `DepthAttachmentDescriptor::Clear(dt)`        | `Clear` (1.0, 0)  | Fresh depth.                                 |
| `DepthAttachmentDescriptor::Keep(dt)`         | `Load`            | Reuse depth produced by a previous pass.     |

All accept an optional `arrayIndex` for cube/array slices.

### MRT pass

```cpp
RenderPassDescriptor pass;
pass.ColorAttachments = {
    ColorAttachmentDescriptor::Clear(_normalsRT->GetRenderTarget(), Colors::Transparent),
    ColorAttachmentDescriptor::Clear(_depthRT->GetRenderTarget(),   Colors::White),
    ColorAttachmentDescriptor::Clear(_emissiveRT->GetRenderTarget(),Colors::Transparent),
};
pass.DepthAttachment = DepthAttachmentDescriptor::Keep(_renderTarget->GetDepthTarget());
pass.DebugLabel      = "G-Buffer";
BeginRenderPass(pass);
```

### Scissor

If you don't set `pass.HasScissor`, the device falls back to **viewport bounds**.
Only set `HasScissor` when you really need a sub-region.

---

## 4. Defining a pipeline

Either inline:

```cpp
RenderPipelineState pso;
pso.ShaderId    = Shader::Ssao;
pso.Blend       = BlendMode::Opaque;
pso.Depth       = DepthState::Write;
pso.Cull        = CullMode::CounterClockwise;
pso.Topology    = PrimitiveType::TriangleList;
pso.InputLayout = _fullScreenVertexInputLayout.get();
BindPipeline(pso);
```

…or via the factories in `Renderer/Graphics/Pipelines.h`:

```cpp
BindPipeline(Pipelines::SortedRoom(_vertexInputLayout.get(), bucket.BlendMode));
```

`BindPipeline` routes through `SetBlendMode` / `SetDepthState` / `SetCullMode` /
`SetPrimitiveType` / `SetInputLayout` / `SetAlphaTest` — each with its own dedup,
so back-to-back identical PSOs cost almost nothing.

### Shader binding caveat

`RenderPipelineState::ShaderId` binds **one** shader. Many fullscreen passes use
a shared VS + per-effect PS. The shared `Shader::PostProcess` is a
`PixelAndVertex` shader; the per-effect ones (`Shader::Ssao`, `Shader::Fxaa`,
`Shader::SmaaXxx`, `Shader::PostProcessDof*`, etc.) are PS-only.

**Bind the common VS once before `BindPipeline`:**

```cpp
_shaders.Bind(Shader::PostProcess);   // VS+PS
BindPipeline(pso);                    // PSO with ShaderId = Shader::Ssao → overrides PS
```

Forgetting this means the PS is the SSAO shader but the VS is whatever was
left bound by the previous pass (often `VSRooms` from G-Buffer), and the
fullscreen triangle is rasterized through a vertex layout it doesn't match → no
output, just the clear value.

---

## 5. Mid-pass depth reset

Vulkan can't `ClearDepthStencilView` inside an active render pass. The portable
way is `Renderer::ClearDepthMidPass(depth, stencil)`, which closes the current
pass and reopens it with depth `LoadAction::Clear`. Color attachments switch to
`Load` automatically so existing color is preserved.

```cpp
DrawHorizonAndSky(...);
ClearDepthMidPass();           // ← splits the pass; stars now z-test independently
DrawStarsAndSun(...);
```

Used today in `DrawHorizonAndSky` (sky → stars) and `DrawBar` / `DrawLoadingBar`
(stencil mask reset).

---

## 6. State caches — what gets reset when

| Trigger                            | Resets                                                 |
| ---------------------------------- | ------------------------------------------------------ |
| `Renderer::BeginRenderPass`        | Texture binding cache (DX11 RTV/SRV hazard prevention) |
| `Renderer::BindRenderTarget(s)`    | Texture binding cache                                  |
| `Renderer::EndRenderPass`          | Render-pass-active flag                                |
| `Renderer::ClearState`             | All texture / pipeline / shader caches                 |
| `Renderer::ResetTextureBindingCache` | Texture binding cache only                           |
| `Renderer::ResetPipelineCache`     | VB / IL / topology / pipeline hash / shader group     |

Anything that ends up calling `_graphicsDevice->ClearState()` directly without
going through `Renderer::ClearState()` will desync the caches and produce a
black screen on the next frame.

---

## 7. Cheat sheet of common patterns

### A) Fullscreen post-process pass

```cpp
BindFullscreenQuadState();                      // shared VS + TriangleList + fullscreen VB/IL

{
    RenderPassDescriptor pass;
    pass.ColorAttachments = { ColorAttachmentDescriptor::Clear(dest, Colors::Black) };
    pass.HasViewport      = true;
    pass.Viewport         = view.Viewport;
    pass.DebugLabel       = "MyEffect";
    BeginRenderPass(pass);
}

_shaders.Bind(Shader::MyEffectPS);              // PS-only, replaces PS
BindRenderTargetAsTexture(TextureRegister::ColorMap, source, SamplerStateRegister::PointWrap);
DrawTriangles(3, 0);

EndRenderPass();
```

### B) MRT G-Buffer-style pass reusing existing depth

```cpp
RenderPassDescriptor pass;
pass.ColorAttachments = {
    ColorAttachmentDescriptor::Clear(rt0, Colors::Transparent),
    ColorAttachmentDescriptor::Clear(rt1, Colors::White),
};
pass.DepthAttachment = DepthAttachmentDescriptor::Keep(mainDepth);   // ← Load
pass.HasViewport     = true;
pass.Viewport        = view.Viewport;
pass.DebugLabel      = "G-Buffer";
BeginRenderPass(pass);

// draw...

EndRenderPass();
```

### C) Per-face cube target

```cpp
for (int face = 0; face < 6; ++face)
{
    RenderPassDescriptor pass;
    pass.ColorAttachments = { ColorAttachmentDescriptor::Clear(cube, Colors::Black, face) };
    pass.DepthAttachment  = DepthAttachmentDescriptor::Clear(cubeDepth, 1.0f, 0, face);
    pass.HasViewport      = true;
    pass.Viewport         = faceViewport;
    pass.DebugLabel       = "Cube Face";
    BeginRenderPass(pass);

    // draw face content...

    EndRenderPass();
}
```

### D) Two-pass effect (compute → blur)

```cpp
{
    RenderPassDescriptor pass;
    pass.ColorAttachments = { ColorAttachmentDescriptor::Clear(_effectRT, Colors::White) };
    pass.HasViewport      = true;
    pass.Viewport         = viewport;
    pass.DebugLabel       = "Effect Compute";
    BeginRenderPass(pass);
}
// bind shader, textures, draw...
EndRenderPass();

{
    RenderPassDescriptor pass;
    pass.ColorAttachments = { ColorAttachmentDescriptor::Clear(_blurredRT, Colors::Transparent) };
    pass.HasViewport      = true;
    pass.Viewport         = viewport;
    pass.DebugLabel       = "Effect Blur";
    BeginRenderPass(pass);
}
// bind blur shader, _effectRT as SRV, draw...
EndRenderPass();
```

---

## 8. Anti-patterns (don't do these)

| ❌ Wrong                                                                | ✅ Right                                                       |
| ---------------------------------------------------------------------- | -------------------------------------------------------------- |
| `_graphicsDevice->BindRenderTarget(rt, dt)`                            | `BindRenderTarget(rt, dt)` (Renderer wrapper)                  |
| `_graphicsDevice->ClearRenderTarget2D(rt, color)`                      | A pass with `ColorAttachmentDescriptor::Clear(rt, color)`     |
| `_graphicsDevice->ClearDepthStencil(dt, ...)` inside a pass            | `ClearDepthMidPass(depth, stencil)`                            |
| `_graphicsDevice->SetPrimitiveType(...)` etc.                          | `SetPrimitiveType(...)` etc. (Renderer wrapper)                |
| `_graphicsDevice->ClearState()`                                        | `ClearState()` (Renderer wrapper resets caches too)            |
| Draw call between `EndRenderPass` and the next `BeginRenderPass`       | Move the draw inside a pass (Vulkan requires it; DX11 quietly tolerates it but the SDL_GPU backend will not) |
| `_shaders.Bind(Shader::SomePS)` without a VS-providing bind first      | Bind `Shader::PostProcess` (or a `PixelAndVertex`) first       |
| Open a pass on RT-A and then draw using RT-B which was bound earlier   | Each pass declares its own targets — never rely on "implicit" RT from a previous pass |
| Read-from-and-write-to the same texture in one pass                    | Two passes: first writes, second reads as SRV                  |

---

## 9. RenderDoc / PIX integration

Every `pass.DebugLabel` becomes an `ID3DUserDefinedAnnotation::BeginEvent` /
`EndEvent` pair in DX11. Captures show the frame as a flat list of named passes:

```
Main Scene Sky
G-Buffer
SSAO
SSAO Blur
Main Scene Opaque/Transparent
HUD 3D
Glow Downscale
Glow Blur Horizontal
Glow Blur Vertical
Glow Combine
SMAA Scene Copy
SMAA Edge Detection
…
Postprocess Final
Back Buffer Composite
```

Always set `DebugLabel` to a short, specific string when you add a pass — it
makes capture inspection trivial.

---

## 10. Vulkan-readiness checklist

If you're about to add a new render path, it's Vulkan-ready when:

1. **Every draw call is inside an active `BeginRenderPass` / `EndRenderPass` scope.**
2. **No `ClearRTV` / `ClearDSV` outside a pass.** Clears must be `LoadAction::Clear` on a pass attachment, or `ClearDepthMidPass` for in-flight resets.
3. **No `BindRenderTarget` outside a pass.** `BeginRenderPass` handles binding.
4. **State (blend / depth / cull / topology / input layout) is applied through Renderer wrappers or via `BindPipeline`.**
5. **A texture is never bound as SRV while it is the active RTV/DSV** (or vice versa). If a target needs to flip role between passes, make sure they're separate `BeginRenderPass` calls so DX11's hazard prevention and our cache invalidation both kick in.
6. **`DebugLabel` is set.** Not strictly required for correctness but mandatory for debuggability.
7. **All resources have a backend-portable format** reachable via `IRenderTarget2D::GetFormat()` / `IDepthTarget::GetFormat()`. If you add a new RT factory, pass the `SurfaceFormat` / `DepthFormat` through.

---

## 11. Where things live

```
TombEngine/Renderer/
├── Renderer.{h,cpp}                       ← wrapper API (use this)
├── Renderer*.cpp                          ← client code (DrawXxx, etc.)
├── Graphics/
│   ├── IGraphicsDevice.h                  ← backend interface
│   ├── RenderPipelineState.h              ← PSO descriptor + hash
│   ├── RenderPassDescriptor.h             ← Pass descriptor + factories + format hash
│   ├── Pipelines.h                        ← pre-defined PSO factories
│   ├── IRenderTarget2D.h / IDepthTarget.h ← target interfaces with GetFormat()
│   └── ...
├── Native/
│   ├── DirectX11/                         ← current backend
│   └── (future) Vulkan / SDL_GPU / Metal
└── ShaderManager/                         ← shader group dedup
```

---

## 12. Future work (not done yet)

- **Push uniforms** vs persistent descriptor sets — architectural decision for the Vulkan backend.
- **Per-attachment blend** in `RenderPipelineState` — only needed when MRT passes want different blend per RT.
- **PSO cache** keyed on `pipeline.Hash() ^ pass.FormatHash() ^ shaderHash` — backend-side, lazy.
- **Resource state tracking** (image layout transitions) — handled automatically by SDL_GPU; pure-Vulkan backend would need barrier insertion.

These don't affect client code: the rendering paths are already shaped to fit them.
