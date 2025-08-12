#include "framework.h"
#include "Objects/Generic/Traps/Pendulum.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/debris.h"
#include "Game/collision/collide_item.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;

namespace TEN::Entities::Generic
{
	void InitializeSwingingSandbag(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.HarmJoints = { 2 };

		const std::vector<unsigned int> SwingingSandbagHarmJoints = { 2 };
		// Set harm joints.
		auto bitField = BitField::Default;
		bitField.Set(SwingingSandbagHarmJoints);
		item.ItemFlags[0] = 2;//bitField.ToPackedBits();
		item.ItemFlags[3] = item.TriggerFlags;
	}

	void InitializeSwingingBox(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.HarmJoints = { 2 };

		const std::vector<unsigned int> SwingingBoxHarmJoints = { 2 };
		// Set harm joints.
		auto bitField = BitField::Default;
		bitField.Set(SwingingBoxHarmJoints);
		item.ItemFlags[0] = 2;//bitField.ToPackedBits();
		item.ItemFlags[3] = item.TriggerFlags;
	}

	void InitializeOverheadPulleyHook(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		const std::vector<unsigned int> OverheadPulleyHookHarmJoints = { 2, 3 };
		// Set harm joints.
		auto bitField = BitField::Default;
		bitField.Set(OverheadPulleyHookHarmJoints);
		item.ItemFlags[0] = bitField.ToPackedBits();


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

		if (!TriggerActive(item))
			return;

		if (item->Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(item, playerItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(*item, *playerItem))
			return;

			if (item->TouchBits.Test(item->ItemFlags[0]))
			{
				DoDamage(playerItem, abs(item->TriggerFlags));

				TriggerLaraBlood();

				if (playerItem->HitPoints > 0)
				{
					ItemPushItem(item, playerItem, coll, false, 1);
				}
			}
		
	}
}
