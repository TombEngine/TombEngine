#include "framework.h"
#include "Objects/TR3/Trap/DrillBit.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/effects/effects.h"
#include "Game/effects/smoke.h"
#include "Game/effects/spark.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Smoke;

namespace TEN::Entities::Traps
{	
	constexpr auto DRILL_BIT_DAMAGE = 70;
	constexpr auto DRILL_BIT_EFFECT_START_FRAME = 6;
	constexpr auto DRILL_BIT_EFFECT_END_FRAME = 84;

	void ControlDrillBit(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		auto pos = Geometry::TranslatePoint(item.Pose.Position, item.Pose.Orientation.y, 510);
		auto targetGameVector = GameVector(pos + Vector3(0, -510, 0), item.RoomNumber);

			if (item.Animation.FrameNumber > DRILL_BIT_EFFECT_START_FRAME &&
				item.Animation.FrameNumber < DRILL_BIT_EFFECT_END_FRAME &&
				item.TriggerFlags)
			{				
				TriggerRicochetSpark(targetGameVector, Random::GenerateAngle(), 2, Vector4(1.0f, 0.9f, 0.1f, 1.0f));
				TriggerRicochetSpark(targetGameVector, Random::GenerateAngle(), 4, Vector4(1.0f, 0.9f, 0.1f, 1.0f));
				SpawnDynamicLight(targetGameVector.x, targetGameVector.y, targetGameVector.z, Random::GenerateInt(4, 12), 24, 16, 4);
			}
			else if (item.TriggerFlags)
			{
				SpawnGunSmokeParticles(targetGameVector.ToVector3(), Vector3::Zero, item.RoomNumber, 0, LaraWeaponType::Pistol, 14);
			}

		AnimateItem(&item);
	}

	void CollideDrillBit(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (item->Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(item, playerItem, coll->Setup.Radius))
			return;

		auto playerBox = GameBoundingBox(playerItem).ToBoundingOrientedBox(playerItem->Pose);
		auto itemBox = GameBoundingBox(item).ToBoundingOrientedBox(item->Pose);

		if (!itemBox.Intersects(playerBox))
			return;
		
		if (playerItem->HitPoints > 0)
		{
			ItemPushItem(item, playerItem, coll, false, 1);
		}

		DoDamage(LaraItem, DRILL_BIT_DAMAGE);
		DoLotsOfBlood(LaraItem->Pose.Position.x, LaraItem->Pose.Position.y - CLICK(2), LaraItem->Pose.Position.z, (short)(item->Animation.Velocity.z * 2), LaraItem->Pose.Orientation.y, LaraItem->RoomNumber, 2);		
	}
}
