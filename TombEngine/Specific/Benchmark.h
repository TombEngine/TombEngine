#pragma once

namespace TEN::Benchmark
{
    constexpr auto BENCHMARK_GPU_PASSES_MAX = 16;

    struct BenchmarkFrameStats
    {
        float CpuFrameTime       = 0.0f; // Milliseconds between consecutive presented frames.
        float CpuRenderTime      = 0.0f; // Milliseconds spent in Renderer::Render before presenting.
        float PresentTime        = 0.0f; // Milliseconds blocked in present (vsync, frame pacing, GPU).
        // GPU milliseconds per pass, a few frames late; negative when unavailable. Names come from the renderer.
        float              GpuPassTimes[BENCHMARK_GPU_PASSES_MAX] = {};
        const char* const* GpuPassNames                           = nullptr;
        int                GpuPassCount                           = 0;
        float SortedCollectTime  = 0.0f;
        float SortedSortTime     = 0.0f;
        float SortedDrawTime     = 0.0f;

        int DrawCalls            = 0;
        int ShadowMapDrawCalls   = 0;
        int Triangles            = 0;
        int ConstantBufferUpdates = 0;
        int PerDrawUpdates       = 0;
        int RoomsDrawn           = 0;
        int SortedFaces          = 0;

        unsigned long long ConstantBufferBytes = 0;
    };

    // Benchmark mode records per-frame renderer statistics to a CSV file in the Logs directory,
    // skips a warmup period, writes a summary to the log and quits the game when the duration elapses.

    void InitializeBenchmark(float durationInSeconds, const std::string& logDirContainingDir);
    void ShutdownBenchmark();
    bool IsBenchmarkActive();
    void UpdateBenchmark();
    void RecordBenchmarkFrame(const BenchmarkFrameStats& stats);

    // True once, at a fixed frame of the recording, so renderers can capture comparable screenshots.
    bool ConsumeBenchmarkScreenshotRequest();
}
