#include "framework.h"
#include "Game/effects/Bubble.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/Ripple.h"
#include "Game/Setup.h"
#include "Objects/objectslist.h"
#include "Math/Math.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Effects::Ripple;
using namespace TEN::Collision::Floordata;
using namespace TEN::Math;

namespace TEN::Effects::Bubble
{
	constexpr auto BUBBLE_COUNT_MAX		   = 1024;
	constexpr auto BUBBLE_COLOR_WHITE	   = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	constexpr auto BUBBLE_LIFE_MAX		   = 30.0f;
	constexpr auto BUBBLE_SIZE_MAX		   = BLOCK(0.5f);
	constexpr auto BUBBLE_OPACTY_MAX	   = 0.8f;
	constexpr auto BUBBLE_OPACTY_MIN	   = BUBBLE_OPACTY_MAX / 2;
	constexpr auto BUBBLE_OSC_VELOCITY_MAX = 0.4f;
	constexpr auto BUBBLE_OSC_VELOCITY_MIN = BUBBLE_OSC_VELOCITY_MAX / 4;

	std::vector<Bubble> Bubbles = {};

	void SpawnBubble(const Vector3& pos, int roomNumber, float size, float amplitude)
	{
		constexpr auto GRAVITY_MAX		 = 12.0f;
		constexpr auto GRAVITY_MIN		 = GRAVITY_MAX * (2 / 3.0f);
		constexpr auto WAVE_VELOCITY_MAX = 1 / 8.0f;
		constexpr auto WAVE_VELOCITY_MIN = WAVE_VELOCITY_MAX / 2;

		if (!TestEnvironment(ENV_FLAG_WATER, roomNumber))
			return;

		auto& bubble = GetNewEffect(Bubbles, BUBBLE_COUNT_MAX);

		size = std::min(size, BUBBLE_SIZE_MAX);

		bubble.SpriteIndex = SPR_BUBBLES;
		bubble.Position =
		bubble.PositionBase = pos;
		bubble.RoomNumber = roomNumber;
		bubble.Color = BUBBLE_COLOR_WHITE;

		bubble.Size =
		bubble.SizeMax = Vector2(size);
		bubble.SizeMin = bubble.Size * 0.7f;

		bubble.Amplitude = Random::GenerateDirection() * amplitude;
		bubble.WavePeriod = Vector3::Zero;
		bubble.WaveVelocity = Vector3(
			Random::GenerateFloat(WAVE_VELOCITY_MIN, WAVE_VELOCITY_MAX),
			Random::GenerateFloat(WAVE_VELOCITY_MIN, WAVE_VELOCITY_MAX),
			Random::GenerateFloat(WAVE_VELOCITY_MIN, WAVE_VELOCITY_MAX));
		
		bubble.Life = std::round(BUBBLE_LIFE_MAX * FPS);
		bubble.OpacityStart = Random::GenerateFloat(BUBBLE_OPACTY_MIN, BUBBLE_OPACTY_MAX);
		bubble.Gravity = Lerp(GRAVITY_MIN, GRAVITY_MAX, size / BUBBLE_SIZE_MAX);
		bubble.OscillationPeriod = Random::GenerateFloat(0.0f, size);
		bubble.OscillationVelocity = Lerp(BUBBLE_OSC_VELOCITY_MAX, BUBBLE_OSC_VELOCITY_MIN, size / BUBBLE_SIZE_MAX);
	}

	void SpawnBubble(const Vector3& pos, int roomNumber, int flags)
	{
		constexpr auto SIZE_LARGE_MAX	  = BUBBLE_SIZE_MAX;
		constexpr auto SIZE_LARGE_MIN	  = SIZE_LARGE_MAX / 2;
		constexpr auto SIZE_SMALL_MAX	  = SIZE_LARGE_MIN / 2;
		constexpr auto SIZE_SMALL_MIN	  = SIZE_SMALL_MAX / 4;
		constexpr auto AMPLITUDE_HIGH_MAX = BLOCK(0.25f);
		constexpr auto AMPLITUDE_LOW_MAX  = AMPLITUDE_HIGH_MAX / 8;

		float size = (flags & (int)BubbleFlags::LargeScale) ?
			Random::GenerateFloat(SIZE_LARGE_MIN, SIZE_LARGE_MAX) :
			Random::GenerateFloat(SIZE_SMALL_MIN, SIZE_SMALL_MAX);
		float amplitude = (flags & (int)BubbleFlags::HighAmplitude) ? AMPLITUDE_HIGH_MAX : AMPLITUDE_LOW_MAX;

		SpawnBubble(pos, roomNumber, size, amplitude);
	}

	void SpawnDiveBubbles(const Vector3& pos, int roomNumber, unsigned int count)
	{
		constexpr auto SPAWN_RADIUS = BLOCK(1 / 32.0f);

		auto sphere = BoundingSphere(pos, SPAWN_RADIUS);
		for (int i = 0; i < count; i++)
		{
			auto bubblePos = Random::GeneratePointInSphere(sphere);
			SpawnBubble(bubblePos, roomNumber, (int)BubbleFlags::HighAmplitude);
		}
	}
	
	void SpawnChaffBubble(const Vector3& pos, int roomNumber)
	{
		constexpr auto CHAFF_BUBBLE_SIZE_MAX = BUBBLE_SIZE_MAX / 8;
		constexpr auto CHAFF_BUBBLE_SIZE_MIN = CHAFF_BUBBLE_SIZE_MAX / 2;
		constexpr auto GRAVITY_MAX			 = 16.0f;
		constexpr auto GRAVITY_MIN			 = GRAVITY_MAX / 4;
		constexpr auto AMPLITUDE_MAX		 = BLOCK(1 / 16.0f);
		constexpr auto WAVE_VELOCITY_MAX	 = 1 / 16.0f;
		constexpr auto WAVE_VELOCITY_MIN	 = WAVE_VELOCITY_MAX / 2;

		auto& bubble = GetNewEffect(Bubbles, BUBBLE_COUNT_MAX);

		float size = Random::GenerateFloat(CHAFF_BUBBLE_SIZE_MIN, CHAFF_BUBBLE_SIZE_MAX);

		bubble.SpriteIndex = SPR_BUBBLES;
		bubble.Position =
		bubble.PositionBase = pos;
		bubble.RoomNumber = roomNumber;
		bubble.Color = BUBBLE_COLOR_WHITE;

		bubble.Size =
		bubble.SizeMax = Vector2(size);
		bubble.SizeMin = bubble.Size * 0.7f;

		bubble.Amplitude = Random::GenerateDirection() * AMPLITUDE_MAX;
		bubble.WavePeriod = Vector3(Random::GenerateFloat(-PI, PI), Random::GenerateFloat(-PI, PI), Random::GenerateFloat(-PI, PI));
		bubble.WaveVelocity = Vector3(
			Random::GenerateFloat(WAVE_VELOCITY_MIN, WAVE_VELOCITY_MAX),
			Random::GenerateFloat(WAVE_VELOCITY_MIN, WAVE_VELOCITY_MAX),
			Random::GenerateFloat(WAVE_VELOCITY_MIN, WAVE_VELOCITY_MAX));
		
		bubble.Life = std::round(BUBBLE_LIFE_MAX * FPS);
		bubble.OpacityStart = Random::GenerateFloat(BUBBLE_OPACTY_MIN, BUBBLE_OPACTY_MAX);
		bubble.Gravity = Lerp(GRAVITY_MIN, GRAVITY_MAX, size / BUBBLE_SIZE_MAX);
		bubble.OscillationPeriod = Random::GenerateFloat(0.0f, size);
		bubble.OscillationVelocity = Lerp(BUBBLE_OSC_VELOCITY_MAX, BUBBLE_OSC_VELOCITY_MIN, size / CHAFF_BUBBLE_SIZE_MAX);
	}

	void UpdateBubbles()
	{
		constexpr auto LIFE_FULL_SCALE		= std::max(BUBBLE_LIFE_MAX - 0.25f, 0.0f);
		constexpr auto LIFE_START_FADING	= std::min(1.0f, BUBBLE_LIFE_MAX);
		constexpr int  ROOM_UPDATE_INTERVAL = 10;

		if (Bubbles.empty())
			return;

		for (auto& bubble : Bubbles)
		{
			if (bubble.Life <= 0.0f)
				continue;

			bubble.StoreInterpolationData();

			int prevRoomNumber = bubble.RoomNumber;

			int updateOffset = (int)(bubble.Position.x + bubble.Position.y + bubble.Position.z) % ROOM_UPDATE_INTERVAL;
			if (GlobalCounter % ROOM_UPDATE_INTERVAL == updateOffset)
			{
				auto roomVector = RoomVector(bubble.RoomNumber, int(bubble.Position.y - bubble.Gravity));
				auto testPosition = Vector3i(bubble.Position.x, bubble.Position.y - bubble.Gravity, bubble.Position.z);
				bubble.RoomNumber = GetRoomVector(roomVector, testPosition).RoomNumber;;
			}

			// Out of water.
			if (bubble.RoomNumber != prevRoomNumber && !TestEnvironment(ENV_FLAG_WATER, bubble.RoomNumber))
			{
				// Hit water surface; spawn ripple.
				SpawnRipple(
					Vector3(bubble.Position.x, g_Level.Rooms[prevRoomNumber].TopHeight, bubble.Position.z),
					bubble.RoomNumber,
					((bubble.SizeMax.x + bubble.SizeMax.y) / 2) * 0.5f,
					(int)RippleFlags::SlowFade);

				bubble.Life = 0.0f;
				continue;
			}
			// Hit ceiling. NOTE: This is a hacky check. New collision fetching should provide fast info on a need-to-know basis.
			else if (bubble.RoomNumber == prevRoomNumber && bubble.Position.y <= g_Level.Rooms[prevRoomNumber].TopHeight)
			{
				bubble.Life = 0.0f;
				continue;
			}

			// Oscillate size according to period.
			bubble.OscillationPeriod += bubble.OscillationVelocity;
			bubble.Size = Vector2(
				(bubble.SizeMin.x / 2) + ((bubble.SizeMax.x - bubble.SizeMin.x) * (0.5f + (0.5f * sin(bubble.OscillationPeriod)))),
				(bubble.SizeMin.y / 2) + ((bubble.SizeMax.y - bubble.SizeMin.y) * (0.5f + (0.5f * cos(bubble.OscillationPeriod + 1.0f)))));
			bubble.Size *= Lerp(0.0f, 1.0f, bubble.Life / std::round(LIFE_FULL_SCALE * FPS));

			// Update opacity.
			float alpha = 1.0f - (bubble.Life / std::round(LIFE_START_FADING * FPS));
			float opacity = Lerp(bubble.OpacityStart, 0.0f, alpha);
			bubble.Color.w = opacity;

			//  TODO: Let bubbles be affected by sinks.
			// Update position.
			bubble.WavePeriod += bubble.WaveVelocity;
			bubble.PositionBase += Vector3(0.0f, -bubble.Gravity, 0.0f);
			bubble.Position = bubble.PositionBase + (bubble.Amplitude * Vector3(sin(bubble.WavePeriod.x), sin(bubble.WavePeriod.y), sin(bubble.WavePeriod.z)));

			// Update life.
			bubble.Life -= 1.0f;
		}
		
		ClearInactiveEffects(Bubbles);
	}

	void ClearBubbles()
	{
		Bubbles.clear();
	}
}
