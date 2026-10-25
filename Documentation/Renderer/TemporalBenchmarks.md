# Temporal Renderer Benchmarks

Measurements taken while adding TAA, GTAO, the automatic render scale and the temporal upscaler (TAAU), used to decide which of these features to keep.

## Setup

- GPUs: discrete NVIDIA RTX 3050 and integrated AMD Radeon (selected with the `lowpower` argument).
- Backends: Vulkan and DX11.
- Levels: `Benchmark_tut1_water`, `Benchmark_karnak`, `Benchmark_coastal`.
- Command line: `debug level Data/<level>.ten benchmark 12 backend <vulkan|dx11> novsync ssao 1 resolution 1920 1080 antialiasing <n> renderscale <n> [lowpower]`.
- Each value is the average frame time in ms reported by the benchmark exit log, averaged over the three levels (runs 1 and 2 also over two repeats per level). p99 values are averaged the same way.
- Base build: `cf8d72784`, the renderer before the temporal work (SSAO, SMAA, FSR 1 below native).
- Image quality was not evaluated, only frame times.

Configuration names:

| Name | Antialiasing | Render scale | Upscaler |
|---|---|---|---|
| base-smaa | SMAA | Native | - |
| base-smaa-fsr67 | SMAA | 67% | FSR 1 |
| smaa-native, taa-native | SMAA / TAA | Native | - |
| smaa-performance | SMAA | 67% | FSR 1 |
| taa-performance (runs 3-4) | TAA | 67% | TAAU |
| taa-performance (run 5) | TAA | 67% | TAA resolve, then FSR 1 |
| taau67, taau50 | TAA | 67% / 50% | TAAU |
| *-automatic (*-dynamic in run 4) | SMAA / TAA | Automatic | As above |

## Run 1 - 2026-10-20, commit 52fb799d7

TAA, GTAO, clustered object lights and dynamic render scale, first measurement.

| Config | Discrete Vulkan | Discrete DX11 | Integrated Vulkan | Integrated DX11 |
|---|---|---|---|---|
| base-smaa | 2.52 | 2.42 | 20.27 | 25.80 |
| new-smaa | 3.01 | 3.00 | 28.64 | 29.83 |
| new-taa | 3.31 | 3.27 | 31.23 | 32.63 |

The new build was 20-40% slower on the integrated GPU with SMAA.

## Run 2 - 2026-10-20, commit ce3598972

Sprites lit once per sprite on the CPU, motion vectors written only with TAA, GTAO filtered in its accumulation pass.

| Config | Discrete Vulkan | Discrete DX11 | Integrated Vulkan | Integrated DX11 |
|---|---|---|---|---|
| base-smaa | 2.53 | 2.40 | 20.33 | 25.96 |
| new-smaa | 2.60 | 2.47 | 20.87 | 26.14 |
| new-taa | 2.98 | 2.82 | 23.70 | 29.20 |

GTAO plus clustered lights now cost about +0.07 ms on the discrete GPU and +0.2-0.5 ms on the integrated GPU. The GTAO pass alone costs about as much as the old SSAO.

## Run 3 - 2026-10-21, commit bd1ce5e96

TAA resolve in a compute shader (344409c5b) and the temporal upscaler (TAAU) with history at window resolution.

| Config | Discrete Vulkan | Discrete DX11 | Integrated Vulkan | Integrated DX11 |
|---|---|---|---|---|
| base-smaa | 2.53 | 2.39 | 20.28 | 25.79 |
| new-smaa | 2.58 | 2.46 | 20.77 | 26.14 |
| new-taa | 2.88 | 2.72 | 23.39 | 28.51 |
| base-fsr67 | 1.66 | 1.52 | 13.11 | 14.40 |
| new-taau67 | 1.89 | 1.76 | 15.35 | 17.31 |
| new-taau50 | 1.51 | 1.41 | 12.36 | 13.15 |

The compute resolve saved 0.3-0.7 ms over the pixel shader resolve on the integrated GPU. TAAU at 67% was 2.2-2.9 ms slower than FSR 1 at 67% on the integrated GPU.

## Run 4 - 2026-10-21, commit a5c071476

Settings reduced to antialiasing none / SMAA / TAA and render scale native / dynamic / performance.

| Config | Discrete Vulkan | Discrete DX11 | Integrated Vulkan | Integrated DX11 |
|---|---|---|---|---|
| base-smaa | 2.52 | 2.40 | 20.57 | 25.78 |
| base-smaa-fsr67 | 1.64 | 1.52 | 13.15 | 14.41 |
| smaa-native | 2.59 | 2.47 | 20.74 | 26.11 |
| taa-native | 2.87 | 2.73 | 23.52 | 28.53 |
| smaa-performance | 1.72 | 1.54 | 13.28 | 14.48 |
| taa-performance (TAAU) | 1.90 | 1.76 | 15.37 | 17.30 |
| smaa-dynamic | 2.60 | 2.47 | 14.31 | 13.68 |
| taa-dynamic | 2.93 | 2.73 | 14.92 | 14.11 |

p99 with the dynamic scale on the integrated GPU: 16.5-17.4 ms, against 14-15 ms at a fixed 67%.

## Run 5 - 2026-10-25, TAAU removed

TAAU reverted: below native the TAA resolve runs at render resolution and FSR 1 upscales after it. The dynamic scale was renamed to automatic and became the default.

| Config | Discrete Vulkan | Discrete DX11 | Integrated Vulkan | Integrated DX11 |
|---|---|---|---|---|
| base-smaa-fsr67 | 1.68 | 1.61 | 13.61 | 14.62 |
| smaa-native | 2.63 | 2.68 | 20.99 | 26.38 |
| taa-native | 2.91 | 3.00 | 23.62 | 28.74 |
| smaa-performance | 1.71 | 1.73 | 13.72 | 14.71 |
| taa-performance (TAA + FSR 1) | 1.84 | 1.85 | 14.62 | 15.52 |
| smaa-automatic | 2.63 | 2.77 | 13.63 | 13.93 |
| taa-automatic | 2.91 | 2.94 | 13.13 | 13.43 |

p99 with the automatic scale on the integrated GPU: 16.1-17.3 ms. The DX11 runs on the discrete GPU had noisy p99 values (3.4-5.1 ms in every configuration, base included), so the Vulkan column is the reference for the discrete GPU.

## Run 6 - 2026-10-25, video memory and HDR

Single runs of 12 seconds at 1920x1080, without the three level average of the earlier runs. Render target memory is the
total the scene renderer logs at exit.

Video memory: TAA and DOF targets created only while active, SMAA and upscale sources pooled (commit 62d845bc5).
`Benchmark_karnak`, discrete GPU, Vulkan:

| Config | Render targets before | Render targets after | Saved |
|---|---|---|---|
| No antialiasing, native | 130.7 MB | 99.1 MB | 31.6 MB |
| SMAA, native | 150.5 MB | 126.7 MB | 23.8 MB |
| TAA, native | 130.7 MB | 122.8 MB | 7.9 MB |
| SMAA, performance | 120.3 MB | 101.8 MB | 18.5 MB |
| TAA, performance | 111.5 MB | 100.0 MB | 11.5 MB |

Screenshots of the new build differ from the old one no more than two runs of the old build differ from each other
(0.5% of the pixels without TAA, animated effects; up to 17% with TAA, whose history depends on the frame timing).

HDR, frame avg in ms and render target memory, HDR off / on:

| Config | Discrete Vulkan, karnak | Integrated Vulkan, coastal | Integrated DX11, coastal |
|---|---|---|---|
| SMAA, native | 2.49 / 2.86 | 22.8 / 25.2 | 27.6 / 29.6 |
| TAA, performance | 1.74 / 1.95 | 15.1 / 16.1 | 16.0 / 16.9 |
| TAA, automatic | - | 14.4 / 12.4 | 11.6 / 12.1 |
| Render targets, SMAA native | 126.7 / 137.3 MB | 136.6 / 165.0 MB | 128.7 / 157.1 MB |

On the integrated GPU at native resolution, HDR costs about 2 ms: the bloom 1.0 ms, the scene pass 0.5 ms (half float
target) and the post-process 0.35 ms. The automatic scale absorbs it. With HDR off, the images match the 8 bit build.

## Conclusions

| Feature | Cost | Decision |
|---|---|---|
| GTAO in place of SSAO | Same as SSAO | Kept |
| Automatic render scale | Halves the integrated GPU frame (26 to 14 ms), stays native on the discrete GPU | Kept, default |
| TAA at native | +0.3 ms discrete, +2.4-2.6 ms (about 10%) integrated | Kept as an option |
| TAA compute resolve | Saves 0.3-0.7 ms integrated | Kept |
| TAAU | 0.8-1.8 ms slower than TAA + FSR 1 at 67% on the integrated GPU | Removed |
| HDR | About 2 ms on the integrated GPU at native, 0.4 ms on the discrete GPU | Kept as an option, off by default |

Below native, TAA followed by FSR 1 costs only 0.8-0.9 ms more than SMAA on the integrated GPU. With the automatic scale, each scale change resets the TAA history, which can cause brief shimmering. A higher quality temporal upscaler is planned as FSR 3.1 (roadmap item R12).

Raw results: `E:\TEN-bench-results.csv` to `E:\TEN-bench-results5.csv` on the benchmark machine.
