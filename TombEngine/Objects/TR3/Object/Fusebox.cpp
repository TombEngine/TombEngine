#include "framework.h"
#include "Objects/TR3/Object/Fusebox.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/effects/effects.h"
#include "Game/effects/spark.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Spark;
using namespace TEN::Math;
using namespace TEN::Math::Random;

namespace TEN::Entities::TR3
{
	// ItemFlags indices.
	enum FuseboxFlags
	{
		IsDestroyed = 0, // 0 = intact, 1 = destroyed.
		SparkTimer  = 1, // Counts down from FUSEBOX_SPARK_DURATION to 0 after destruction.
		FlashTimer  = 2  // Brief countdown for the initial destruction flash.
	};

	// Spark effect parameters.
	constexpr auto FUSEBOX_SPARK_DURATION = 10 * FPS;

	// Dynamic lighting parameters.
	constexpr auto FUSEBOX_FLASH_DURATION  = FPS / 2;
	constexpr auto FUSEBOX_FLASH_FALLOFF   = BLOCK(4);
	constexpr auto FUSEBOX_FLICKER_FALLOFF = BLOCK(2);

	static void UpdateDynamicLight(const ItemInfo& item, const Vector3i& pos, int sparkTimer, int flashTimer)
	{
		// Bright white-blue flash immediately after destruction.
		if (flashTimer > 0)
		{
			float flashIntensity = (float)flashTimer / FUSEBOX_FLASH_DURATION;

			SpawnDynamicPointLight(
				pos.ToVector3(),
				Color(0.6f * flashIntensity, 0.8f * flashIntensity, 1.0f * flashIntensity),
				FUSEBOX_FLASH_FALLOFF,
				false, item.Index);
			return;
		}

		if (sparkTimer <= 0)
			return;

		float intensity = (float)sparkTimer / FUSEBOX_SPARK_DURATION;
		float flicker   = GenerateFloat(0.1f, 1.0f) * intensity;

		SpawnDynamicPointLight(
			pos.ToVector3(),
			Color(0.15f * flicker, 0.25f * flicker, 0.5f * flicker),
			FUSEBOX_FLICKER_FALLOFF,
			false, item.Index);
	}

	void InitializeFusebox(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[FuseboxFlags::IsDestroyed] = 0;
		item.ItemFlags[FuseboxFlags::SparkTimer]  = 0;
		item.ItemFlags[FuseboxFlags::FlashTimer]  = 0;
	}

	void ControlFusebox(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		auto pos = Vector3i(GameBoundingBox(ID_FUSEBOX_SWITCH).ToBoundingOrientedBox(item.Pose).Center);

		// Already destroyed; run spark wind-down effects.
		if (item.ItemFlags[FuseboxFlags::IsDestroyed] == 1)
		{
			int sparkTimer = item.ItemFlags[FuseboxFlags::SparkTimer];
			int flashTimer = item.ItemFlags[FuseboxFlags::FlashTimer];

			if (flashTimer > 0)
			{
				flashTimer--;
				item.ItemFlags[FuseboxFlags::FlashTimer] = flashTimer;
			}

			if (sparkTimer > 0)
			{
				float intensity = (float)sparkTimer / FUSEBOX_SPARK_DURATION;

				TriggerFuseboxContinuousSparks(pos, item.RoomNumber, intensity);
				UpdateDynamicLight(item, pos, sparkTimer, flashTimer);

				if (TestProbability(0.5f * intensity))
					SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &item.Pose);

				sparkTimer--;
				item.ItemFlags[FuseboxFlags::SparkTimer] = sparkTimer;
			}
			else
			{
				// Deactivate the item after sparks have finished.
				item.Status = ITEM_NOT_ACTIVE;
			}

			AnimateItem(&item);
			return;
		}

		// Check if fusebox was just activated by gunfire (ProcessShootSwitch sets IFLAG_SWITCH_ONESHOT).
		if (item.Flags & IFLAG_SWITCH_ONESHOT)
		{
			item.ItemFlags[FuseboxFlags::IsDestroyed] = 1;
			item.ItemFlags[FuseboxFlags::SparkTimer]  = FUSEBOX_SPARK_DURATION;
			item.ItemFlags[FuseboxFlags::FlashTimer]  = FUSEBOX_FLASH_DURATION;

			SetAnimation(item, item.ObjectNumber, 1);
			TriggerFuseboxDestructionBlast(pos, item.RoomNumber);
			SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &item.Pose);
		}

		AnimateItem(&item);
	}

	void CollideFusebox(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		ObjectCollision(itemNumber, laraItem, coll);
	}
}
