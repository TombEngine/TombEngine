#include "framework.h"
#include "Specific/Benchmark.h"

#include <mutex>

#include "Specific/clock.h"

namespace TEN::Benchmark
{
    constexpr auto BENCHMARK_WARMUP_TIME = 3.0f;
    constexpr auto BENCHMARK_FLUSH_FRAMES = 60;
    constexpr auto BENCHMARK_SCREENSHOT_GAME_FRAME = 240;
    constexpr auto BENCHMARK_SCREENSHOT_INTERVAL   = 240; // One screenshot per profiling flyby sequence.
    constexpr auto BENCHMARK_SCREENSHOT_SKIPPED_FRAMES = 4; // The capture stalls the frame and the readback writes its file frames later.
    constexpr auto BENCHMARK_MENU_SCREENSHOT_TICK = 90;
    constexpr auto BENCHMARK_MENU_EXIT_TICK       = 150;
    constexpr auto BENCHMARK_HEADER_WAIT_FRAMES   = 16; // Frames to wait for the first GPU timings, which are read a few frames late.

    static bool _isActive = false;
    static bool _isStarted = false;
    static bool _isFinished = false;
    static float _duration = 0.0f;
    static std::chrono::steady_clock::time_point _startTime = {};
    static int _recordedFrames = 0;
    static int _headerWaitFrames = 0;
    static int  _screenshotCount = 0;
    static int  _screenshotSkippedFrames = 0;
    static int  _menuStartTick     = -1;
    static bool _isMenuCaptured    = false;
    static bool _isQuitRequested   = false;
    static std::ofstream _file;
    static std::mutex _mutex;
    static std::vector<float> _frameTimes = {};
    static std::vector<float> _renderTimes = {};
    static unsigned long long _drawCallSum = 0;
    static unsigned long long _uniformByteSum = 0;

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
        _file << "frame,time_s,cpu_frame_ms,cpu_render_ms,present_ms,rooms,sorted_faces,frontend_sorted_collect_ms,frontend_sorted_sort_ms,"
                 "backend_cpu_ms,draw_calls,shadow_draw_calls,triangles,uniform_pushes,uniform_bytes,upload_bytes,"
                 "sorted_collect_ms,sorted_record_ms";

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
            "draw calls avg {}; uniform bytes avg {} KB.",
            _recordedFrames,
            averageFrameTime, GetPercentile(_frameTimes, 0.5f), GetPercentile(_frameTimes, 0.99f),
            averageRenderTime, GetPercentile(_renderTimes, 0.99f),
            _drawCallSum / _recordedFrames, (_uniformByteSum / _recordedFrames) / 1024), LogLevel::Info);
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
        if (!_isActive || gameFrame < (BENCHMARK_SCREENSHOT_GAME_FRAME + (_screenshotCount * BENCHMARK_SCREENSHOT_INTERVAL)))
            return false;

        _screenshotCount++;
        _screenshotSkippedFrames = BENCHMARK_SCREENSHOT_SKIPPED_FRAMES;
        return true;
    }

    // Title and inventory frames do not run the game loop that ends the benchmark: they are captured after a fixed
    // number of logic ticks, and the run then ends.
    std::string GetBenchmarkScreenshotName()
    {
        return ((_screenshotCount <= 1) ? std::string("Benchmark.png") : fmt::format("Benchmark_{}.png", _screenshotCount - 1));
    }

    bool ConsumeMenuScreenshotRequest()
    {
        if (!_isActive)
            return false;

        // Menus are timed in logic ticks, so the capture does not depend on the framerate.
        int tick = (int)g_Synchronizer.GetStepCount();
        if (_menuStartTick < 0)
            _menuStartTick = tick;

        int menuTicks = tick - _menuStartTick;
        if (menuTicks >= BENCHMARK_MENU_EXIT_TICK && !_isQuitRequested)
        {
            _isQuitRequested = true;
            auto quitEvent = SDL_Event{};
            quitEvent.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quitEvent);
        }

        if (menuTicks < BENCHMARK_MENU_SCREENSHOT_TICK || _isMenuCaptured)
            return false;

        _isMenuCaptured = true;
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

        // Frames of the screenshot capture are not representative of the scene.
        if (_screenshotSkippedFrames > 0)
        {
            _screenshotSkippedFrames--;
            return;
        }

        // GPU timings arrive some frames late: the header waits for them, so the CSV gets its per pass columns.
        if (_recordedFrames == 0 && stats.GpuPassCount == 0 && _headerWaitFrames < BENCHMARK_HEADER_WAIT_FRAMES)
        {
            _headerWaitFrames++;
            return;
        }

        if (_recordedFrames == 0)
            WriteHeader(stats);

        _file << _recordedFrames << ',' << time << ','
              << stats.CpuFrameTime << ',' << stats.CpuRenderTime << ',' << stats.PresentTime << ','
              << stats.RoomsDrawn << ',' << stats.SortedFaces << ','
              << stats.FrontendSortedCollectTime << ',' << stats.FrontendSortedSortTime << ','
              << stats.BackendTime << ',' << stats.DrawCalls << ',' << stats.ShadowDrawCalls << ',' << stats.Triangles << ','
              << stats.UniformPushes << ',' << stats.UniformBytes << ',' << stats.UploadBytes << ','
              << stats.SortedCollectTime << ',' << stats.SortedRecordTime;

        for (int i = 0; i < stats.GpuPassCount; i++)
            _file << ',' << stats.GpuPassTimes[i];

        for (int i = 0; i < stats.GpuPassCount; i++)
            _file << ',' << stats.CpuPassTimes[i];

        _file << '\n';

        _frameTimes.push_back(stats.CpuFrameTime);
        _renderTimes.push_back(stats.CpuRenderTime);
        _drawCallSum += stats.DrawCalls;
        _uniformByteSum += stats.UniformBytes;
        _recordedFrames++;

        if ((_recordedFrames % BENCHMARK_FLUSH_FRAMES) == 0)
            _file.flush();
    }
}
