#include "framework.h"
#include "Objects/TR2/Trap/OverheadPulleyHook.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/effects.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;

namespace TEN::Entities::Traps
{
	constexpr int FIRST_DAMAGE_FRAME_START = 54;
	constexpr int FIRST_DAMAGE_FRAME_END = 66;
	constexpr int SECOND_DAMAGE_FRAME_START = 114;

	const std::vector<unsigned int> OverheadPulleyHookHarmJoints = { 2, 3 };

	void ControlOverheadPulleyHook(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		if ((item.Animation.FrameNumber > FIRST_DAMAGE_FRAME_START && item.Animation.FrameNumber < FIRST_DAMAGE_FRAME_END) ||
			(item.Animation.FrameNumber > SECOND_DAMAGE_FRAME_START))
		{
			item.ItemFlags[3] = 0;
		}
		else
		{
			item.ItemFlags[3] = item.TriggerFlags;
		}

		AnimateItem(&item);
	}

	void CollideOverheadPulleyHook(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(item, playerItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(*item, *playerItem))
			return;

		for (int i = 0; i < OverheadPulleyHookHarmJoints.size(); i++)
		{
			if (item->TouchBits.Test(OverheadPulleyHookHarmJoints[i]))
			{
				if (playerItem->HitPoints > 0)
				{
					ItemPushItem(item, playerItem, coll, false, 0);
				}

				if (!TriggerActive(item))
					return;

				if (item->ItemFlags[3])
				{
					DoDamage(playerItem, abs(item->ItemFlags[3]));
					TriggerLaraBlood();
				}
			}
		}
	}
}