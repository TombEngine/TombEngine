#include "framework.h"
#include "Objects/TR4/Trap/tr4_blade.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Specific/level.h"

namespace TEN::Entities::Traps
{
	void CollideBlade(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& bladeItem = g_Level.Items[itemNumber];

		if (bladeItem.Status == ITEM_INVISIBLE)
			return;

		if (!bladeItem.ItemFlags[3])
			return;

		if (!TestBoundsCollide(&bladeItem, playerItem, coll->Setup.Radius))
			return;

		auto prevPos = playerItem->Pose.Position;

		if (!ItemPushItem(&bladeItem, playerItem, coll, true, 1))
			return;

		DoDamage(playerItem, bladeItem.ItemFlags[3]);

		auto deltaPos = prevPos - playerItem->Pose.Position;
		if (deltaPos != Vector3i::Zero && TriggerActive(&bladeItem))
		{
			DoBloodSplat(
				(GetRandomControl() & 0x3F) + playerItem->Pose.Position.x - 32,
				playerItem->Pose.Position.y - (GetRandomControl() & 0x1FF) - 256,
				(GetRandomControl() & 0x3F) + playerItem->Pose.Position.z - 32,
				(GetRandomControl() & 3) + (bladeItem.ItemFlags[3] / 32) + 2,
				Random::GenerateAngle(),
				playerItem->RoomNumber);
		}

		if (!coll->Setup.EnableObjectPush)
			playerItem->Pose.Position += deltaPos;
	}
}
