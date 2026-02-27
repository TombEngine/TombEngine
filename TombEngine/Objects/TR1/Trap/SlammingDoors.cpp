#include "framework.h"
#include "Objects/TR1/Trap/SlammingDoors.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/effects/spark.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"

using namespace TEN::Effects::Spark;
using namespace TEN::Math;
using namespace TEN::Math::Random;

namespace TEN::Entities::Traps
{
	constexpr auto SLAMMING_DOORS_HARM_DAMAGE = 400;

	enum SlammingDoorsState
	{
		SLAMMING_DOORS_DISABLED = 0,
		SLAMMING_DOORS_ENABLED = 1
	};

	enum SlammingDoorsAnim
	{
		SLAMMING_DOORS_ANIM_OPENED = 0,
		SLAMMING_DOORS_ANIM_CLOSING = 1,
		SLAMMING_DOORS_ANIM_OPENING = 2
	};

	void InitializeSlammingDoors(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		SetAnimation(item, SLAMMING_DOORS_ANIM_OPENED);

		// Used by GenericSphereBoxCollision. Bits correspond to joint damage index.
		// 3 = 000000000 000000011, so damage joints are 1 and 2 (both doors).
		item.ItemFlags[0] = 0;

		// Used by GenericSphereBoxCollision for trap damage value.
		item.ItemFlags[3] = SLAMMING_DOORS_HARM_DAMAGE;
	}

	void ControlSlammingDoors(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			if (item.Animation.TargetState != SLAMMING_DOORS_ENABLED)
			{
				item.Animation.TargetState = SLAMMING_DOORS_ENABLED;
				item.ItemFlags[0] = 3;
			}
		}
		else
		{
			if (item.Animation.TargetState != SLAMMING_DOORS_DISABLED)
			{
				item.Animation.TargetState = SLAMMING_DOORS_DISABLED;
				item.ItemFlags[0] = 0;
			}
		}

		const auto& anim = GetAnimData(item);


		if (IsSoundEffectCommandActive(item, SFX_TR1_SLAMDOOR_CLOSE) && item.TriggerFlags)
		{
			SpawnSlammingDoorSparks(Vector3i(130, 0, 560), item);   // right door
			SpawnSlammingDoorSparks(Vector3i(-130, 0, 560), item);  // left door
		}

		AnimateItem(&item);
	}

	void SpawnSlammingDoorSparks(const Vector3i& localOffset, const ItemInfo& item)
	{
		static const std::array<std::pair<int, int>, 6> yOffsetRanges = 
		{
			std::pair{-800, -900},
			std::pair{-600, -700},
			std::pair{-500, -600},
			std::pair{-400, -500},
			std::pair{-300, -400},
			std::pair{-200, -300}
		};

		const auto basePos = Geometry::TranslatePoint(item.Pose.Position, item.Pose.Orientation.y, localOffset);
		const Vector4 sparkColor(1.0f, 0.5f, 0.1f, 1.0f);

		for (const auto& [minY, maxY] : yOffsetRanges)
		{
			const auto y = Random::GenerateInt(minY, maxY);
			const auto sparkPos = basePos + Vector3(0, y, 0);
			const auto sparkTarget = GameVector(sparkPos, item.RoomNumber);
			TriggerSlammingDoorSpark(sparkTarget, Random::GenerateAngle(), 3, sparkColor);
		}
	}

	void TriggerSlammingDoorSpark(const GameVector& pos, short angle, int count, const Vector4& colorStart)
	{
		for (int i = 0; i < count; i++)
		{
			auto& s = GetFreeSparkParticle();
			s = {};
			s.age = 0;
			s.life = GenerateFloat(5, 10);
			s.friction = 0.98f;
			s.gravity = 1.2f;
			s.width = 9.0f;
			s.height = 85.0f;
			s.room = pos.RoomNumber;
			s.pos = pos.ToVector3();
			float ang = TO_RAD(angle);
			Vector3 v = Vector3(sin(ang + GenerateFloat(-PI_DIV_2, PI_DIV_2)), GenerateFloat(-1, 1), cos(ang + GenerateFloat(-PI_DIV_2, PI_DIV_2)));
			v += Vector3(GenerateFloat(-64, 64), GenerateFloat(-64, 64), GenerateFloat(-64, 64));
			v.Normalize(v);
			s.velocity = v * GenerateFloat(17, 24);
			s.sourceColor = colorStart;
			s.destinationColor = Vector4::Zero;
			s.active = true;
		}
	}
}
