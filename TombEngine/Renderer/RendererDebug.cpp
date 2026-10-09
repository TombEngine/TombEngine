#include "framework.h"
#include "Renderer/Renderer.h"

#include "Renderer/Next/NextRenderer.h"

namespace TEN::Renderer
{
	void Renderer::ResetDebugVariables()
	{
		_timeUpdate = 0;
		_timeDraw = 0;
		_timeFrame = 0;
		_timeRoomsCollector = 0;
		_timeSortedCollect = 0.0f;
		_timeSortedSort = 0.0f;
		_timeSortedDraw = 0.0f;
		_numDrawCalls = 0;

		_numRoomsDrawCalls = 0;
		_numSortedRoomsDrawCalls = 0;
		_numMoveablesDrawCalls = 0;
		_numSortedMoveablesDrawCalls = 0;
		_numStaticsDrawCalls = 0;
		_numInstancedStaticsDrawCalls = 0;
		_numSortedStaticsDrawCalls = 0;
		_numSpritesDrawCalls = 0;
		_numInstancedSpritesDrawCalls = 0;
		_numSortedSpritesDrawCalls = 0;
		
		_numLinesDrawCalls = 0;

		_numTriangles = 0;
		_numSortedTriangles = 0;

		_numShadowMapDrawCalls = 0;
		_numDebrisDrawCalls = 0;
		_numEffectsDrawCalls = 0;

		_numDotProducts = 0;
		_numCheckPortalCalls = 0;
		_numGetVisibleRoomsCalls = 0;

		_numConstantBufferUpdates = 0;
        _numConstantBufferBytes = 0;
        _numPerDrawUpdates = 0;
		_numInterleavedSortedGroups = 0;

		_numRequestedMaterialsUpdates = 0;
		_numExecutedMaterialsUpdates = 0;

		_currentLineHeight;
	}

	void Renderer::CollectFrameStats(const RenderView& view)
	{
		if (!TEN::Benchmark::IsBenchmarkActive())
			return;

		_frameStats.SortedCollectTime = _timeSortedCollect;
		_frameStats.SortedSortTime = _timeSortedSort;
		_frameStats.SortedDrawTime = _timeSortedDraw;
		_frameStats.DrawCalls = _numDrawCalls;
		_frameStats.ShadowMapDrawCalls = _numShadowMapDrawCalls;
		_frameStats.Triangles = _numTriangles;
		_frameStats.ConstantBufferUpdates = _numConstantBufferUpdates;
		_frameStats.ConstantBufferBytes = _numConstantBufferBytes;
		_frameStats.PerDrawUpdates = _numPerDrawUpdates;
		_frameStats.RoomsDrawn = (int)view.RoomsToDraw.size();
		_frameStats.SortedFaces = (int)view.TransparentSortKeys.size();
	}

	void Renderer::RecordBenchmarkFrame(std::chrono::steady_clock::time_point renderStartTime, std::chrono::steady_clock::time_point presentStartTime)
	{
		if (!TEN::Benchmark::IsBenchmarkActive())
			return;

		using ms = std::chrono::duration<float, std::milli>;

		auto now = std::chrono::steady_clock::now();
		_frameStats.CpuRenderTime = ms(presentStartTime - renderStartTime).count();
		_frameStats.PresentTime = ms(now - presentStartTime).count();
		_frameStats.GpuPassNames = GPU_TIMING_SCOPE_NAMES;
		_frameStats.GpuPassCount = (int)GpuTimingScope::Count;
		for (int i = 0; i < _frameStats.GpuPassCount; i++)
		{
			_frameStats.GpuPassTimes[i] = _graphicsDevice->GetGpuTime((GpuTimingScope)i);
			_frameStats.CpuPassTimes[i] = _graphicsDevice->GetCpuTime((GpuTimingScope)i);
		}
		_frameStats.CpuFrameTime = (_lastPresentTime.time_since_epoch().count() == 0) ? 0.0f : ms(now - _lastPresentTime).count();
		_lastPresentTime = now;

		// The preview frame is recorded inside RenderScene; keep the legacy render time free of it.
		const auto& nextStats = Next::g_NextRenderer.GetFrameStats();
		_frameStats.NextRenderTime = Next::g_NextRenderer.IsActive() ? nextStats.CpuTime : 0.0f;
		_frameStats.NextDrawCalls = Next::g_NextRenderer.IsActive() ? nextStats.DrawCalls : 0;
		_frameStats.CpuRenderTime -= _frameStats.NextRenderTime;

		TEN::Benchmark::RecordBenchmarkFrame(_frameStats);
	}

	void Renderer::PrintDebugMessage(const char* msg, va_list args)
	{
		constexpr auto LINE_X_POS	= DISPLAY_SPACE_RES.x / 100;
		constexpr auto LINE_SPACING = DISPLAY_SPACE_RES.y / 30;
		constexpr auto COLOR		= Color(1.0f, 1.0f, 1.0f);
		constexpr auto SCALE		= 0.8f;

		char buffer[255];
		vsnprintf(buffer, sizeof(buffer), msg, args);
		AddString(buffer, Vector2(LINE_X_POS, _currentLineHeight), COLOR, SCALE, (int)PrintStringFlags::Outline);

		_currentLineHeight += LINE_SPACING;
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
