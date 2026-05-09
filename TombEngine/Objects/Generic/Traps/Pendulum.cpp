#include "framework.h"
#include "Objects/Generic/Traps/Pendulum.h"

#include "Game/collision/Sphere.h"
#include "Game/collision/collide_item.h"
#include "Game/effects/effects.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;

namespace TEN::Entities::Generic
{
	void InitializePendulum(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[0] = 2;
		item.ItemFlags[3] = item.TriggerFlags;
	}

	void ControlPendulum(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
			AnimateItem(&item);
	}

	void CollidePendulum(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(item, playerItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(*item, *playerItem))
			return;

		if (item->TouchBits.Test(item->ItemFlags[0]))
		{
			if (playerItem->HitPoints > 0)
			{
				ItemPushItem(item, playerItem, coll, false, 1);
			}

			if (!TriggerActive(item))
				return;

			DoDamage(playerItem, abs(item->TriggerFlags));

			TriggerLaraBlood();
		}
	}
}
