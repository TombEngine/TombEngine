#include "framework.h"
#include "Renderer/Renderer.h"

namespace TEN::Renderer
{
	void Renderer::ResetDebugVariables()
	{
		_timeUpdate = 0;
		_timeDraw = 0;
		_timeFrame = 0;
		_timeRoomsCollector = 0;
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

		_currentLineHeight;
	}

	void Renderer::PrintDebugMessage(LPCSTR msg, va_list args)
	{
		constexpr auto LINE_X_POS	= DISPLAY_SPACE_RES.x / 100;
		constexpr auto LINE_SPACING = DISPLAY_SPACE_RES.y / 30;
		constexpr auto COLOR		= Color(1.0f, 1.0f, 1.0f);
		constexpr auto SCALE		= 0.8f;

		char buffer[255];
		ZeroMemory(buffer, 255);
		_vsprintf_l(buffer, msg, nullptr, args);
		AddString(buffer, Vector2(LINE_X_POS, _currentLineHeight), COLOR, SCALE, (int)PrintStringFlags::Outline);

		_currentLineHeight += LINE_SPACING;
	}

	void Renderer::PrintDebugMessage(LPCSTR msg, ...)
	{
		auto args = va_list{};
		va_start(args, msg);
		PrintDebugMessage(msg, args);
		va_end(args);
	}
}
