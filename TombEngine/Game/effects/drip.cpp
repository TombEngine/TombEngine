#include "framework.h"
#include "Game/effects/Drip.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/effects/Ripple.h"
#include "Game/effects/weather.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Ripple;
using namespace TEN::Collision::Floordata;
using namespace TEN::Math;

namespace TEN::Effects::Drip
{
	constexpr auto DRIP_COUNT_MAX	= 1024;
	constexpr auto DRIP_COLOR_WHITE = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	std::vector<Drip> Drips = {};

	void SpawnDrip(const Vector3& pos, int roomNumber, const Vector3& velocity, float lifeInSec, float gravity)
	{
		constexpr auto WIDTH = 4.0f;

		auto& drip = GetNewEffect(Drips, DRIP_COUNT_MAX);

		drip.Position = pos;
		drip.RoomNumber = roomNumber;
		drip.Velocity = velocity;
		drip.Size = Vector2(WIDTH, 0.0f);
		drip.Color = DRIP_COLOR_WHITE;
		drip.Life =
		drip.LifeMax = std::round(lifeInSec * FPS);
		drip.Gravity = gravity;
	}

	void SpawnSplashDrips(const Vector3& pos, int roomNumber, unsigned int count, bool isSmallSplash)
	{
		constexpr auto LIFE_MAX					  = 4.0f;
		constexpr auto VELOCITY_BASE			  = 16.0f;
		constexpr auto VERTICAL_VELOCITY_HIGH_MAX = 64.0f;
		constexpr auto VERTICAL_VELOCITY_HIGH_MIN = 32.0f;
		constexpr auto VERTICAL_VELOCITY_LOW_MAX  = 24.0f;
		constexpr auto VERTICAL_VELOCITY_LOW_MIN  = 16.0f;
		constexpr auto SPAWN_RADIUS_LARGE		  = BLOCK(1 / 8.0f);
		constexpr auto SPAWN_RADIUS_SMALL		  = BLOCK(1 / 64.0f);

		// TODO: Can spawn beneath water surface.
		auto sphere = BoundingSphere(pos, isSmallSplash ? SPAWN_RADIUS_SMALL : SPAWN_RADIUS_LARGE);

		for (int i = 0; i < count; i++)
		{
			auto dripPos = Random::GeneratePointInSphere(sphere);
			auto direction = dripPos - pos;
			direction.Normalize();

			float verticalVel = isSmallSplash ?
				Random::GenerateFloat(VERTICAL_VELOCITY_LOW_MIN, VERTICAL_VELOCITY_LOW_MAX) :
				Random::GenerateFloat(VERTICAL_VELOCITY_HIGH_MIN, VERTICAL_VELOCITY_HIGH_MAX);
			auto vel = (direction * VELOCITY_BASE) + Vector3(0.0f, -verticalVel, 0.0f);

			float systemGravity = g_GameFlow->GetSettings()->Physics.Gravity;

			float gravity = isSmallSplash ?
				Random::GenerateFloat(systemGravity / 3, systemGravity / 2) :
				Random::GenerateFloat(systemGravity / 2, systemGravity);

			SpawnDrip(dripPos, roomNumber, vel, LIFE_MAX, gravity);
		}
	}

	void SpawnWetnessDrip(const Vector3& pos, int roomNumber)
	{
		constexpr auto LIFE_MAX		= 1.0f;
		constexpr auto SPAWN_RADIUS = BLOCK(1 / 32.0f);

		auto gravity = g_GameFlow->GetSettings()->Physics.Gravity;
		auto sphere = BoundingSphere(pos, SPAWN_RADIUS);
		auto dripPos = Random::GeneratePointInSphere(sphere);
		SpawnDrip(dripPos, roomNumber, Vector3::Zero, LIFE_MAX, Random::GenerateFloat(gravity / 2, gravity));
	}

	void UpdateDrips()
	{
		constexpr auto RIPPLE_SIZE_WATER_MAX  = 24.0f;
		constexpr auto RIPPLE_SIZE_WATER_MIN  = 16.0f;
		constexpr auto RIPPLE_SIZE_GROUND_MAX = 16.0f;
		constexpr auto RIPPLE_SIZE_GROUND_MIN = 8.0f;
		constexpr auto RIPPLE_HEIGHT_OFFSET	  = 4;

		if (Drips.empty())
			return;

		for (auto& drip : Drips)
		{
			if (drip.Life <= 0.0f)
				continue;

			drip.StoreInterpolationData();

			// Update velocity.
			drip.Velocity.y += drip.Gravity;
			if (TestEnvironment(ENV_FLAG_WIND, drip.RoomNumber))
				drip.Velocity += Weather.Wind();

			int prevRoomNumber = drip.RoomNumber;
			auto pointColl = GetPointCollision(drip.Position, drip.RoomNumber);

			// Update position.
			drip.Position += drip.Velocity;
			drip.RoomNumber = pointColl.GetRoomNumber();

			// Update size and color.
			float alpha = 1.0f - (drip.Life / drip.LifeMax);
			drip.Size.y = Lerp(drip.Size.x / (1 / 6.4f), 0.0f, alpha);
			drip.Color = Vector4::Lerp(DRIP_COLOR_WHITE, Vector4::Zero, alpha);

			// Hit water.
			if (TestEnvironment(ENV_FLAG_WATER, drip.RoomNumber))
			{
				// Spawn ripple on surface only.
				if (!TestEnvironment(ENV_FLAG_WATER, prevRoomNumber))
				{
					SpawnRipple(
						Vector3(drip.Position.x, pointColl.GetWaterTopHeight() - RIPPLE_HEIGHT_OFFSET, drip.Position.z),
						pointColl.GetRoomNumber(),
						Random::GenerateFloat(RIPPLE_SIZE_WATER_MIN, RIPPLE_SIZE_WATER_MAX),
						(int)RippleFlags::SlowFade | (int)RippleFlags::LowOpacity);
				}

				drip.Life = 0.0f;
				continue;
			}
			// Hit floor; spawn ripple.
			else if (drip.Position.y >= pointColl.GetFloorHeight())
			{
				SpawnRipple(
					Vector3(drip.Position.x, pointColl.GetFloorHeight() - RIPPLE_HEIGHT_OFFSET, drip.Position.z),
					pointColl.GetRoomNumber(),
					Random::GenerateFloat(RIPPLE_SIZE_GROUND_MIN, RIPPLE_SIZE_GROUND_MAX),
					(int)RippleFlags::SlowFade | (int)RippleFlags::LowOpacity | (int)RippleFlags::OnGround,
					pointColl.GetFloorNormal());

				drip.Life = 0.0f;
				continue;
			}
			// Hit ceiling; deactivate.
			else if (drip.Position.y <= pointColl.GetCeilingHeight())
			{
				drip.Life = 0.0f;
				continue;
			}

			// Update life.
			drip.Life -= 1.0f;
		}

		ClearInactiveEffects(Drips);
	}

	void ClearDrips()
	{
		Drips.clear();
	}
}
