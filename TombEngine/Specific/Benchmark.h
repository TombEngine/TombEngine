#pragma once

namespace TEN::Benchmark
{
    constexpr auto BENCHMARK_GPU_PASSES_MAX = 16;

    struct BenchmarkFrameStats
    {
        float CpuFrameTime       = 0.0f; // Milliseconds between consecutive presented frames.
        float CpuRenderTime      = 0.0f; // Milliseconds spent in Renderer::Render before presenting, without the scene renderer recording.
        float PresentTime        = 0.0f; // Milliseconds blocked in present (vsync, frame pacing, GPU).
        // GPU milliseconds per pass, a few frames late; negative when unavailable. Names come from the renderer.
        float              GpuPassTimes[BENCHMARK_GPU_PASSES_MAX] = {};
        float              CpuPassTimes[BENCHMARK_GPU_PASSES_MAX] = {}; // CPU milliseconds of the same scopes.
        const char* const* GpuPassNames                           = nullptr;
        int                GpuPassCount                           = 0;
        int   RoomsDrawn                = 0;
        int   SortedFaces               = 0;
        float FrontendSortedCollectTime = 0.0f; // Milliseconds collecting and sorting the sorted faces of the view.
        float FrontendSortedSortTime    = 0.0f;

        float     BackendTime       = 0.0f; // Milliseconds spent recording the frame in the scene renderer.
        int       DrawCalls         = 0;
        int       ShadowDrawCalls   = 0;
        long long Triangles         = 0;
        int       UniformPushes     = 0;
        long long UniformBytes      = 0;
        int       UploadBytes       = 0;
        float     SortedCollectTime = 0.0f;
        float     SortedRecordTime  = 0.0f;
    };

    // Benchmark mode records per-frame renderer statistics to a CSV file in the Logs directory,
    // skips a warmup period, writes a summary to the log and quits the game when the duration elapses.

    void InitializeBenchmark(float durationInSeconds, const std::string& logDirContainingDir);
    void ShutdownBenchmark();
    bool IsBenchmarkActive();
    void UpdateBenchmark();
    void RecordBenchmarkFrame(const BenchmarkFrameStats& stats);

    // True once, when the game frame counter reaches a fixed value, so renderers and backends capture
    // the same game state for comparison.
    bool ConsumeBenchmarkScreenshotRequest(int gameFrame);
    bool ConsumeMenuScreenshotRequest();
}
