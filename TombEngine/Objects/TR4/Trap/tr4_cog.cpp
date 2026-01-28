#include "framework.h"
#include "Objects/TR4/Trap/tr4_cog.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Traps
{
	void ControlCog(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			item.Status = ITEM_ACTIVE;
			AnimateItem(item);
		}
		else if (item.TriggerFlags == 2)
		{
			item.Status = ITEM_INVISIBLE;
		}
	}

	void CollideCog(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& cogItem = g_Level.Items[itemNumber];
		
		if (cogItem.Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(&cogItem, playerItem, coll->Setup.Radius))
			return;

		if (TriggerActive(&cogItem))
		{
			DoBloodSplat(
				(GetRandomControl() & 0x3F) + playerItem->Pose.Position.x - 32, 
				(GetRandomControl() & 0x1F) + cogItem.Pose.Position.y - 16, 
				(GetRandomControl() & 0x3F) + playerItem->Pose.Position.z - 32, 
				(GetRandomControl() & 3) + 2, 
				Random::GenerateAngle(),
				playerItem->RoomNumber);

			DoDamage(playerItem, 10);
		}
		else if (coll->Setup.EnableObjectPush)
		{
			ItemPushItem(&cogItem, playerItem, coll, false, 0);
		}
	}
}
