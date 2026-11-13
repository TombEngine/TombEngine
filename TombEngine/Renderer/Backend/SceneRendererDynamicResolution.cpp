// Dynamic resolution. With the dynamic render scale setting, the scene resolution follows the GPU load to hold the target
// frame time: the scale drops when frames are late and the game thread waits for the GPU (the frame fences of
// WaitForFrameInFlight), and rises again after a while without waiting. Changes recreate the scene targets, so they are
// rate limited, and a rise that is quickly undone doubles the wait before the next one. Below 1, FSR 1 upscales the
// scene to the window resolution as with the fixed render scales.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

namespace TEN::Renderer::Backend
{
    constexpr auto DYNAMIC_SCALE_MIN          = 0.5f;
    constexpr auto DYNAMIC_SCALE_MAX          = 1.0f;
    constexpr auto DYNAMIC_SCALE_STEP_DOWN    = 0.1f;
    constexpr auto DYNAMIC_SCALE_STEP_UP      = 0.05f;
    constexpr auto DYNAMIC_WINDOW_FRAMES      = 30;      // Frames averaged per decision.
    constexpr auto DYNAMIC_LATE_FACTOR        = 1.05f;   // Frames this much over the target are late.
    constexpr auto DYNAMIC_GPU_BOUND_WAIT     = 0.5f;    // Average fence wait, in milliseconds, of a GPU bound frame.
    constexpr auto DYNAMIC_IDLE_WAIT          = 0.25f;   // Average fence wait below which the GPU has headroom.
    constexpr auto DYNAMIC_DOWN_DELAY         = 0.5f;    // Seconds between two drops.
    constexpr auto DYNAMIC_UP_DELAY           = 2.0f;    // Seconds of headroom before a rise.
    constexpr auto DYNAMIC_UP_DELAY_MAX       = 32.0f;
    constexpr auto DYNAMIC_FAILED_RISE_PERIOD = 3.0f;    // A drop this soon after a rise doubles the rise delay.

    // Render scale of the frame for the dynamic setting. Called once per scene frame, after the fence wait.
    float SceneRenderer::UpdateDynamicResolution(float targetFrameTime)
    {
        auto now = std::chrono::steady_clock::now();
        if (_dynamicLastFrameTime != std::chrono::steady_clock::time_point{})
        {
            _dynamicFrameTimeSum += GetMilliseconds(_dynamicLastFrameTime, now);
            _dynamicFenceWaitSum += _fenceWaitTime;
            _dynamicFrameCount++;
        }

        _dynamicLastFrameTime = now;
        if (_dynamicFrameCount < DYNAMIC_WINDOW_FRAMES)
            return _dynamicScale;

        float frameTime = _dynamicFrameTimeSum / _dynamicFrameCount;
        float fenceWait = _dynamicFenceWaitSum / _dynamicFrameCount;
        _dynamicFrameTimeSum = 0.0f;
        _dynamicFenceWaitSum = 0.0f;
        _dynamicFrameCount = 0;

        float sinceChange = std::chrono::duration<float>(now - _dynamicChangeTime).count();
        float scale = _dynamicScale;

        bool isGpuBound = (frameTime > targetFrameTime * DYNAMIC_LATE_FACTOR) && (fenceWait > DYNAMIC_GPU_BOUND_WAIT);
        if (isGpuBound && sinceChange >= DYNAMIC_DOWN_DELAY && scale > DYNAMIC_SCALE_MIN)
        {
            scale = std::max(scale - DYNAMIC_SCALE_STEP_DOWN, DYNAMIC_SCALE_MIN);

            // The last rise did not hold: wait longer before the next one.
            if (_isDynamicRise && sinceChange < DYNAMIC_FAILED_RISE_PERIOD)
                _dynamicUpDelay = std::min(_dynamicUpDelay * 2.0f, DYNAMIC_UP_DELAY_MAX);

            _isDynamicRise = false;
        }
        else if (!isGpuBound && fenceWait < DYNAMIC_IDLE_WAIT && sinceChange >= _dynamicUpDelay && scale < DYNAMIC_SCALE_MAX)
        {
            scale = std::min(scale + DYNAMIC_SCALE_STEP_UP, DYNAMIC_SCALE_MAX);
            _isDynamicRise = true;
        }
        else if (sinceChange >= DYNAMIC_UP_DELAY_MAX)
        {
            // A long stable period forgets the failed rises.
            _dynamicUpDelay = DYNAMIC_UP_DELAY;
        }

        if (scale != _dynamicScale)
        {
            TENLog(fmt::format("Dynamic resolution: render scale {:.0f}% (frame {:.2f} ms, target {:.2f} ms, GPU wait {:.2f} ms).",
                scale * 100.0f, frameTime, targetFrameTime, fenceWait), LogLevel::Info);

            _dynamicScale = scale;
            _dynamicChangeTime = now;
        }

        return _dynamicScale;
    }

    // Starts the next dynamic period at full scale, after a level change or a switch to the dynamic setting.
    void SceneRenderer::ResetDynamicResolution()
    {
        _dynamicScale = DYNAMIC_SCALE_MAX;
        _dynamicUpDelay = DYNAMIC_UP_DELAY;
        _isDynamicRise = false;
        _dynamicFrameTimeSum = 0.0f;
        _dynamicFenceWaitSum = 0.0f;
        _dynamicFrameCount = 0;
        _dynamicLastFrameTime = {};
        _dynamicChangeTime = std::chrono::steady_clock::now();
    }
}
