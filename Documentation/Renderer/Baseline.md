# Renderer baseline

Measurements of the current DX11 renderer, used as reference for every phase of `RendererPlan.md`.

## How to run

```
Build\<Config>\Bin\x64\TombEngine.exe debug level Data/<level>.ten benchmark <seconds>
```

Working directory: `Build\<Config>`. The level must be registered in `Scripts/Gameflow.lua`; the benchmark
scenario script (`Scripts/Levels/Benchmark.lua`, a copy is in `Documentation/Renderer/Benchmark.lua`) keeps the
player alive and turns her slowly, so the chase camera sweeps the scene. Since 2026-10-09 it also emits additive
and alpha blended particles around the player, so the sprite and sorted transparency paths are measured too. Timing starts with the first rendered gameplay frame, the first 3 seconds are
discarded, then per-frame data goes to `Logs/Benchmark_<timestamp>.csv` and a summary to the TEN log.

CSV columns:
- `cpu_render_ms`: CPU time in `Renderer::Render` before present. When the GPU is the bottleneck, the driver
  blocks the CPU inside rendering calls, so this value then also contains GPU wait time.
- `present_ms`: time blocked in present (vsync, frame cap).
- `gpu_<pass>_ms`: GPU time per pass from DX11 timestamp queries, read 4 frames later.
- `cb_bytes`: constant buffer data uploaded per frame.

Benchmark levels: `Benchmark_tut1_water`, `Benchmark_karnak`, `Benchmark_coastal`. Karnak and Coastal are
compiled from `Build/Debug/TE Test Levels/*.prj2` with the headless TombLib compiler (see the recompile
commands in the session notes); their textures, sky and sounds are taken from `E:\trle`.

## 2026-10-08, Release x64, AMD Radeon integrated GPU, 1728x1083

These runs were labeled RTX 3050 at first, but the legacy renderer created its device on DXGI adapter 0, which is
the integrated GPU on this hybrid laptop (the NvOptimusEnablement export was not honored). `3cd8e79f4` makes the
legacy renderer request the high performance adapter; see the 2026-10-09 RTX 3050 section.

### Before SSAO optimization (`8aebc26e0` without `0a200191e`)

| Level | GPU frame | SSAO | Post-process | Opaque | GBuffer | Sky | Draw calls | CB/frame |
|---|---|---|---|---|---|---|---|---|
| tut1_water | 30.4 ms | 21.3 ms | 4.7 ms | 2.1 ms | 0.8 ms | 0.6 ms | 123 | 144 KB |
| karnak | 30.3 ms | 21.3 ms | 4.6 ms | 2.1 ms | 0.8 ms | 0.6 ms | 122 | 145 KB |
| coastal | 35.7 ms | 26.2 ms | 4.5 ms | 3.3 ms | 1.1 ms | 0.4 ms | 226 | 321 KB |

### After SSAO optimization (`0a200191e`, 32 samples, separable blur)

| Level | GPU frame | SSAO | Post-process | Opaque | GBuffer | Sky |
|---|---|---|---|---|---|---|
| tut1_water | 20.1 ms | 10.8 ms | 4.9 ms | 2.1 ms | 0.8 ms | 0.6 ms |
| karnak | 20.0 ms | 10.8 ms | 4.9 ms | 2.1 ms | 0.8 ms | 0.6 ms |
| coastal | 23.1 ms | 13.3 ms | 4.7 ms | 3.3 ms | 1.1 ms | 0.4 ms |

With the GPU saturated, the 30 FPS cap is not reached and `present_ms` drops to about 0.2 ms.
In an earlier run with the GPU below its limit, CPU render time was 0.42-0.50 ms (p99 1.1-1.8 ms).

## 2026-10-09, Release x64, RTX 3050, 1728x1083, with particles

With `3cd8e79f4` both renderers run on the RTX 3050. Legacy renderer alone, GPU per pass:

| Level | GPU frame | SSAO | Opaque | Transparent | Post-process | GBuffer | Sky | CPU render | Sorted faces |
|---|---|---|---|---|---|---|---|---|---|
| coastal | 3.43 ms | 1.09 ms | 0.85 ms | 0.28 ms | 0.69 ms | 0.27 ms | 0.06 ms | 3.3 ms | 178 |
| karnak | 3.27 ms | 0.92 ms | 0.72 ms | 0.38 ms | 0.71 ms | 0.19 ms | 0.12 ms | 3.0 ms | 5674 |
| tut1_water | 3.51 ms | 0.92 ms | 0.71 ms | 0.41 ms | 0.78 ms | 0.31 ms | 0.12 ms | 3.6 ms | 5388 |

On the discrete GPU the legacy renderer is CPU bound: about 3 ms of CPU per frame, of which the sorted faces
(collect, sort, draw) take about 0.3 ms.

Next renderer preview on DX11 (`preview dx11`, GPU time from the DX11 RHI timestamps, logged at exit). It has rooms,
objects, player, sky, sprites, sorted faces, materials and SSAO, but no shadows or post-process yet:

| Level | GPU frame | Depth and normals prepass | SSAO (half resolution) | Scene | CPU per frame | Draw calls |
|---|---|---|---|---|---|---|
| coastal | 1.53 ms | 0.14 ms | 0.19 ms | 1.13 ms | 0.10 ms | 187 |
| karnak | 1.43 ms | 0.11 ms | 0.15 ms | 1.11 ms | 0.12 ms | 107 |
| tut1_water | 1.79 ms | 0.24 ms | 0.33 ms | 1.01 ms | 0.12 ms | 108 |

Half resolution SSAO costs 0.15-0.33 ms against 0.92-1.09 ms for the legacy full resolution pass.
With a preview open both renderers share the GPU, so legacy timings in those runs are higher.

## 2026-10-09, Release x64, coastal with particles (legacy on the integrated GPU)

Legacy renderer alone, GPU per pass (DX11 timestamps):

| GPU frame | SSAO | Opaque | Transparent | Post-process | GBuffer | Sky | Draw calls |
|---|---|---|---|---|---|---|---|
| 31.5 ms | 13.3 ms | 7.9 ms | 4.4 ms | 4.2 ms | 1.0 ms | 0.4 ms | 229 |

Next renderer preview (`preview <backend>`) on the same frames, rooms, objects, player, sky, sprites and sorted faces
(no SSAO, shadows or post-process yet). CPU time covers collection, upload and command recording:

| Backend | CPU per frame | Draw calls |
|---|---|---|
| Vulkan (RTX 3050) | 0.91 ms | 111 |
| DX11 (RTX 3050) | 0.55 ms | 111 |

The legacy `cpu_render_ms` in these runs excludes the preview time (`next_cpu_ms` column). The preview devices were
still created with debug layers in these runs; without them the CPU time is 0.10-0.15 ms.

## Findings

- On the integrated GPU the legacy renderer is GPU bound and SSAO dominates (54-58% of the GPU frame). On the
  RTX 3050 it is CPU bound (about 3 ms per frame) and the GPU frame is 3.3-3.5 ms.
- Requesting the high performance adapter (`3cd8e79f4`) is by far the largest gain on hybrid laptops: about 10x.
- Half resolution SSAO in the next renderer is 3-6x cheaper than the legacy full resolution pass.
- Post-process costs about 0.7 ms on the RTX 3050 and needs its own breakdown (distortion, DOF, glow, SMAA, final pass).
- Shadows, god rays and dust storm are inactive in these scenes.

## Limitations

- Starting views show 4-29 rooms; scenes with more geometry and transparency are still needed.
- GPU timing is DX11 only; SDL GPU has no timestamp queries (use RenderDoc, Nsight or RGP for Vulkan).
- `previewlowpower` runs the preview on the integrated GPU, for comparisons on that GPU.
