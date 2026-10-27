#include "framework.h"
#include "Objects/TR3/Trap/SpikedFrame.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Specific/level.h"

namespace TEN::Entities::Traps
{	
	constexpr auto SPIKED_FRAME_DAMAGE = 800;
	constexpr auto SPIKED_FRAME_DAMAGE_STATE = 1;

	void ControlSPikedFrame(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		item.Animation.TargetState = SPIKED_FRAME_DAMAGE_STATE;

		AnimateItem(&item);
	}

	void CollideSpikedFrame(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
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

		if (item->Animation.ActiveState != SPIKED_FRAME_DAMAGE_STATE)
			return;

		DoDamage(playerItem, SPIKED_FRAME_DAMAGE);
		DoLotsOfBlood(playerItem->Pose.Position.x, playerItem->Pose.Position.y - CLICK(2), playerItem->Pose.Position.z, (short)(item->Animation.Velocity.z * 2), playerItem->Pose.Orientation.y, playerItem->RoomNumber, 2);
	}
}
