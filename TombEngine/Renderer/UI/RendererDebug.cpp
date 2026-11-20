#include "framework.h"
#include "Renderer/Renderer.h"

#include "Renderer/Backend/SceneRenderer.h"

// Frontend statistics for the debug window and the benchmark, and debug messages of the game code.

namespace TEN::Renderer
{
	// Clears the per-frame frontend counters and timings.
	void Renderer::ResetDebugVariables()
	{
		_timeUpdate = 0;
		_timeFrame = 0;
		_timeRoomsCollector = 0;
		_timeSortedCollect = 0.0f;
		_timeSortedSort = 0.0f;

		_numDotProducts = 0;
		_numCheckPortalCalls = 0;
		_numGetVisibleRoomsCalls = 0;
		_numInterleavedSortedGroups = 0;
	}

	// Keeps the view counts for the debug window and, during a benchmark, the frontend statistics of the frame.
	void Renderer::CollectFrameStats(const RenderView& view)
	{
		_debugViewCounts = DebugViewCounts{ (int)view.RoomsToDraw.size(), (int)view.SpritesToDraw.size(), (int)view.LightsToDraw.size(), (int)view.FogBulbsToDraw.size(), (int)view.TransparentSortKeys.size() };

		if (!TEN::Benchmark::IsBenchmarkActive())
			return;

		_frameStats.FrontendSortedCollectTime = _timeSortedCollect;
		_frameStats.FrontendSortedSortTime = _timeSortedSort;
		_frameStats.RoomsDrawn = (int)view.RoomsToDraw.size();
		_frameStats.SortedFaces = (int)view.TransparentSortKeys.size();
	}

	// GPU time per pass, sampled about once a second and a few frames late, with the CPU recording time of the same
	// pass in this frame. Reading does not turn the measurement on: the benchmark does, unless nogputiming is set.
	void Renderer::FillBenchmarkPassTimes(const Backend::SceneFrameStats& stats)
	{
		const auto& timings = Backend::g_SceneRenderer.SampleGpuTimings(false);

		int count = std::min((int)timings.size(), TEN::Benchmark::BENCHMARK_GPU_PASSES_MAX);
		for (int i = 0; i < count; i++)
		{
			_benchmarkPassNames[i] = timings[i].Name.c_str();
			_frameStats.GpuPassTimes[i] = timings[i].Milliseconds;
			_frameStats.CpuPassTimes[i] = -1.0f;
			for (const auto& pass : stats.Passes)
			{
				if (pass.Name != nullptr && timings[i].Name == pass.Name)
				{
					_frameStats.CpuPassTimes[i] = pass.CpuTime;
					break;
				}
			}
		}

		_frameStats.GpuPassNames = _benchmarkPassNames;
		_frameStats.GpuPassCount = count;
	}

	// Records the CPU, present and scene renderer timings and counters of the frame in the benchmark.
	void Renderer::RecordBenchmarkFrame(std::chrono::steady_clock::time_point renderStartTime, std::chrono::steady_clock::time_point presentStartTime)
	{
		if (!TEN::Benchmark::IsBenchmarkActive())
			return;

		using ms = std::chrono::duration<float, std::milli>;

		auto now = std::chrono::steady_clock::now();
		_frameStats.CpuRenderTime = ms(presentStartTime - renderStartTime).count();
		_frameStats.PresentTime = ms(now - presentStartTime).count();
		_frameStats.CpuFrameTime = (_lastPresentTime.time_since_epoch().count() == 0) ? 0.0f : ms(now - _lastPresentTime).count();
		_lastPresentTime = now;

		// The scene renderer records its frame inside RenderScene; the render time leaves it out.
		if (Backend::g_SceneRenderer.IsActive())
		{
			const auto& stats = Backend::g_SceneRenderer.GetFrameStats();
			_frameStats.BackendTime = stats.CpuTime;
			_frameStats.DrawCalls = stats.DrawCalls;
			_frameStats.ShadowDrawCalls = stats.CategoryDrawCalls[(int)Backend::DrawCategory::Shadows];
			_frameStats.Triangles = stats.Api.Triangles;
			_frameStats.UniformPushes = stats.Api.UniformPushes;
			_frameStats.UniformBytes = stats.Api.UniformBytes;
			_frameStats.UploadBytes = stats.UploadBytes;
			_frameStats.SortedCollectTime = stats.SortedCollectTime;
			_frameStats.SortedRecordTime = stats.SortedRecordTime;
			FillBenchmarkPassTimes(stats);
		}

		_frameStats.CpuRenderTime -= _frameStats.BackendTime;

		TEN::Benchmark::RecordBenchmarkFrame(_frameStats);
	}

	// Debug messages are listed by the ImGui debug window.
	void Renderer::PrintDebugMessage(const char* msg, va_list args)
	{
		char buffer[255];
		vsnprintf(buffer, sizeof(buffer), msg, args);
		_debugMessages.push_back(buffer);
	}

	void Renderer::PrintDebugMessage(const char* msg, ...)
	{
		if (_isLocked)
			return;

		va_list args;
		va_start(args, msg);
		PrintDebugMessage(msg, args);
		va_end(args);
	}
}
