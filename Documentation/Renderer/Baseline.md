# Renderer baseline

Measurements of the current DX11 renderer, used as reference for every phase of `RendererPlan.md`.

## How to run

```
Build\<Config>\Bin\x64\TombEngine.exe debug level Data/<level>.ten benchmark <seconds>
```

Working directory: `Build\<Config>`. The level must be registered in `Scripts/Gameflow.lua`; the benchmark
scenario script (`Scripts/Levels/Benchmark.lua`) keeps the player alive and turns her slowly, so the chase
camera sweeps the scene. Timing starts with the first rendered gameplay frame, the first 3 seconds are
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

## 2026-10-08, Release x64, RTX 3050, 1728x1083

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

## Findings

- The renderer is GPU-bound on this hardware. CPU submission is small (about 0.5 ms) for these scenes.
- SSAO is the dominant cost (54-58% of the GPU frame even after optimization). Candidates: half resolution
  with bilateral upsample, fewer samples with per-pixel rotation, smaller radius in screen space.
- Post-process costs about 4.8 ms and needs its own breakdown (distortion, DOF, glow, SMAA, final pass).
- Shadows, god rays and dust storm are inactive in these scenes.

## Limitations

- Starting views show 4-29 rooms; scenes with more geometry and transparency are still needed.
- GPU timing is DX11 only; SDL GPU has no timestamp queries (use RenderDoc, Nsight or RGP for Vulkan).
