#include "framework.h"
#include "Specific/Benchmark.h"

#include <mutex>

namespace TEN::Benchmark
{
    constexpr auto BENCHMARK_WARMUP_TIME = 3.0f;
    constexpr auto BENCHMARK_FLUSH_FRAMES = 60;
    constexpr auto BENCHMARK_SCREENSHOT_GAME_FRAME = 240;

    static bool _isActive = false;
    static bool _isStarted = false;
    static bool _isFinished = false;
    static float _duration = 0.0f;
    static std::chrono::steady_clock::time_point _startTime = {};
    static int _recordedFrames = 0;
    static bool _isScreenshotTaken = false;
    static std::ofstream _file;
    static std::mutex _mutex;
    static std::vector<float> _frameTimes = {};
    static std::vector<float> _renderTimes = {};
    static unsigned long long _drawCallSum = 0;
    static unsigned long long _constantBufferByteSum = 0;

    static std::string GetBenchmarkTimestamp()
    {
        auto now = std::time(nullptr);
        auto localTime = std::tm{};

#ifdef SDL_PLATFORM_WIN32
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif

        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &localTime);
        return buffer;
    }

    static float GetPercentile(std::vector<float>& values, float percentile)
    {
        if (values.empty())
            return 0.0f;

        std::sort(values.begin(), values.end());
        int index = std::clamp((int)(percentile * (values.size() - 1)), 0, (int)values.size() - 1);
        return values[index];
    }

    static float GetAverage(const std::vector<float>& values)
    {
        if (values.empty())
            return 0.0f;

        double sum = 0.0;
        for (float value : values)
            sum += value;

        return (float)(sum / values.size());
    }

    static float GetElapsedTime()
    {
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - _startTime).count();
    }

    static void WriteHeader(const BenchmarkFrameStats& stats)
    {
        _file << "frame,time_s,cpu_frame_ms,cpu_render_ms,present_ms,draw_calls,shadow_draw_calls,triangles,"
                 "cb_updates,cb_bytes,perdraw_updates,rooms,sorted_faces,sorted_collect_ms,sorted_sort_ms,sorted_draw_ms,"
                 "next_cpu_ms,next_draw_calls";

        for (int i = 0; i < stats.GpuPassCount; i++)
            _file << ",gpu_" << stats.GpuPassNames[i] << "_ms";

        for (int i = 0; i < stats.GpuPassCount; i++)
            _file << ",cpuscope_" << stats.GpuPassNames[i] << "_ms";

        _file << '\n';
    }

    static void WriteSummary()
    {
        if (_recordedFrames == 0)
        {
            TENLog("Benchmark finished without recorded frames.", LogLevel::Warning);
            return;
        }

        float averageFrameTime = GetAverage(_frameTimes);
        float averageRenderTime = GetAverage(_renderTimes);

        TENLog(fmt::format(
            "Benchmark summary ({} frames): frame avg {:.3f} ms, p50 {:.3f} ms, p99 {:.3f} ms; render avg {:.3f} ms, p99 {:.3f} ms; "
            "draw calls avg {}; CB bytes avg {} KB.",
            _recordedFrames,
            averageFrameTime, GetPercentile(_frameTimes, 0.5f), GetPercentile(_frameTimes, 0.99f),
            averageRenderTime, GetPercentile(_renderTimes, 0.99f),
            _drawCallSum / _recordedFrames, (_constantBufferByteSum / _recordedFrames) / 1024), LogLevel::Info);
    }

    void InitializeBenchmark(float durationInSeconds, const std::string& logDirContainingDir)
    {
        auto logDir = std::filesystem::path(logDirContainingDir) / "Logs";
        std::filesystem::create_directories(logDir);

        auto filePath = logDir / ("Benchmark_" + GetBenchmarkTimestamp() + ".csv");
        _file.open(filePath);
        if (!_file.is_open())
        {
            TENLog("Benchmark disabled: cannot create " + filePath.string(), LogLevel::Error);
            return;
        }

        _isActive = true;
        _isStarted = false;
        _isFinished = false;
        _duration = durationInSeconds;
        _recordedFrames = 0;
        _frameTimes.reserve(10000);
        _renderTimes.reserve(10000);

        TENLog(fmt::format("Benchmark started: {:.1f} s after {:.1f} s warmup, output {}.", durationInSeconds, BENCHMARK_WARMUP_TIME, filePath.string()), LogLevel::Info);
    }

    void ShutdownBenchmark()
    {
        auto lock = std::lock_guard<std::mutex>(_mutex);

        if (!_isActive)
            return;

        WriteSummary();
        _file.close();
        _isActive = false;
    }

    bool IsBenchmarkActive()
    {
        return _isActive;
    }

    bool ConsumeBenchmarkScreenshotRequest(int gameFrame)
    {
        if (!_isActive || _isScreenshotTaken || gameFrame < BENCHMARK_SCREENSHOT_GAME_FRAME)
            return false;

        _isScreenshotTaken = true;
        return true;
    }

    void UpdateBenchmark()
    {
        {
            auto lock = std::lock_guard<std::mutex>(_mutex);

            if (!_isActive || !_isStarted || _isFinished)
                return;

            if (GetElapsedTime() < (BENCHMARK_WARMUP_TIME + _duration))
                return;

            _isFinished = true;
        }

        ShutdownBenchmark();

        auto quitEvent = SDL_Event{};
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
    }

    void RecordBenchmarkFrame(const BenchmarkFrameStats& stats)
    {
        auto lock = std::lock_guard<std::mutex>(_mutex);

        if (!_isActive || _isFinished)
            return;

        // Timing starts with the first rendered gameplay frame, so level loading is excluded.
        if (!_isStarted)
        {
            _startTime = std::chrono::steady_clock::now();
            _isStarted = true;
        }

        float time = GetElapsedTime() - BENCHMARK_WARMUP_TIME;
        if (time < 0.0f || time > _duration)
            return;

        if (_recordedFrames == 0)
            WriteHeader(stats);

        _file << _recordedFrames << ',' << time << ','
              << stats.CpuFrameTime << ',' << stats.CpuRenderTime << ',' << stats.PresentTime << ','
              << stats.DrawCalls << ',' << stats.ShadowMapDrawCalls << ',' << stats.Triangles << ','
              << stats.ConstantBufferUpdates << ',' << stats.ConstantBufferBytes << ',' << stats.PerDrawUpdates << ','
              << stats.RoomsDrawn << ',' << stats.SortedFaces << ','
              << stats.SortedCollectTime << ',' << stats.SortedSortTime << ',' << stats.SortedDrawTime << ','
              << stats.NextRenderTime << ',' << stats.NextDrawCalls;

        for (int i = 0; i < stats.GpuPassCount; i++)
            _file << ',' << stats.GpuPassTimes[i];

        for (int i = 0; i < stats.GpuPassCount; i++)
            _file << ',' << stats.CpuPassTimes[i];

        _file << '\n';

        _frameTimes.push_back(stats.CpuFrameTime);
        _renderTimes.push_back(stats.CpuRenderTime);
        _drawCallSum += stats.DrawCalls;
        _constantBufferByteSum += stats.ConstantBufferBytes;
        _recordedFrames++;

        if ((_recordedFrames % BENCHMARK_FLUSH_FRAMES) == 0)
            _file.flush();
    }
}
