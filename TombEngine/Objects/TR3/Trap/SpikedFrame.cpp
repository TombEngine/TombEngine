#include "framework.h"
#include "Objects/TR3/Trap/SpikedFrame.h"

#include "Game/animation.h"
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

		if (itemBox.Intersects(playerBox))
		{
			if (playerItem->HitPoints > 0)
			{
				ItemPushItem(item, playerItem, coll, false, 1);
			}

			if (item->Animation.ActiveState != SPIKED_FRAME_DAMAGE_STATE)
				return;

			DoDamage(LaraItem, SPIKED_FRAME_DAMAGE);
			DoLotsOfBlood(LaraItem->Pose.Position.x, LaraItem->Pose.Position.y - CLICK(2), LaraItem->Pose.Position.z, (short)(item->Animation.Velocity.z * 2), LaraItem->Pose.Orientation.y, LaraItem->RoomNumber, 2);
		}
	}
}
