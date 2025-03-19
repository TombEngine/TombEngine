#include "framework.h"
#include "Objects/TR4/Object/tr4_scales.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/control/control.h"
#include "Game/effects/Drip.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Objects/TR4/Entity/tr4_ahmet.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Drip;
using namespace TEN::Entities::Switches;
using namespace TEN::Entities::TR4;

ObjectCollisionBounds ScalesBounds =
{
	GameBoundingBox(
		-CLICK(5.5f), -CLICK(5.5f),
		0, 0,
		-BLOCK(0.5f), BLOCK(0.5f)),
	std::pair(
		EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
		EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f)))
};

void ScalesControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (TestLastFrame(*item))
	{
		AnimateItem(*item);
		return;
	}

	if (item->Animation.ActiveState == 1 || item->ItemFlags[1])
	{
		if (true/*Objects[item->ObjectNumber].animIndex*/) // TODO: Check. Not clear what the intent is.
		{
			RemoveActiveItem(itemNumber);
			item->Status = ITEM_NOT_ACTIVE;
			item->ItemFlags[1] = 0;

			AnimateItem(*item);
			return;
		}

		if (RespawnAhmet(Lara.Context.InteractedItem))
		{
			short itemNos[8];
			int sw = GetSwitchTrigger(item, itemNos, 0);

			if (sw > 0)
			{
				while (g_Level.Items[itemNos[sw]].ObjectNumber == ID_FLAME_EMITTER2)
				{
					if (--sw <= 0)
						break;
				}

				g_Level.Items[itemNos[sw]].Flags = 1024;
			}

			item->Animation.TargetState = 1;
		}

		AnimateItem(*item);
	}

	int flags = 0;

	if (item->Animation.ActiveState == 2)
	{
		flags = -512;
		RemoveActiveItem(itemNumber);
		item->Status = ITEM_NOT_ACTIVE;
	}
	else
	{
		flags = -1024;
		item->ItemFlags[1] = 1;
	}

	TestTriggers(item, true, flags);
	AnimateItem(*item);
}

void ScalesCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	ItemInfo* item = &g_Level.Items[itemNumber];

	if (TestBoundsCollide(item, laraItem, LARA_RADIUS))
	{
		if (laraItem->Animation.AnimNumber != LA_WATERSKIN_POUR_LOW && laraItem->Animation.AnimNumber != LA_WATERSKIN_POUR_HIGH || item->Animation.ActiveState != 1)
		{
			GlobalCollisionBounds.X1 = 640;
			GlobalCollisionBounds.X2 = 1280;
			GlobalCollisionBounds.Y1 = -1280;
			GlobalCollisionBounds.Y2 = 0;
			GlobalCollisionBounds.Z1 = -256;
			GlobalCollisionBounds.Z2 = 384;

			ItemPushItem(item, laraItem, coll, false, 2);

			GlobalCollisionBounds.X1 = -256;
			GlobalCollisionBounds.X2 = 256;

			ItemPushItem(item, laraItem, coll, false, 2);

			GlobalCollisionBounds.X1 = -1280;
			GlobalCollisionBounds.X2 = -640;

			ItemPushItem(item, laraItem, coll, false, 2);
		}
		else
		{
			short rotY = item->Pose.Orientation.y;
			item->Pose.Orientation.y = (short)(laraItem->Pose.Orientation.y + ANGLE(45.0f)) & 0xC000;

			ScalesBounds.BoundingBox.X1 = -1408;
			ScalesBounds.BoundingBox.X2 = -640;
			ScalesBounds.BoundingBox.Z1 = -512;
			ScalesBounds.BoundingBox.Z2 = 0;

			if (TestLaraPosition(ScalesBounds, item, laraItem))
			{
				laraItem->Animation.AnimNumber = LA_WATERSKIN_POUR_HIGH;
				laraItem->Animation.FrameNumber = 0;
				item->Pose.Orientation.y = rotY;
			}
			else if (laraItem->Animation.FrameNumber == 51)
			{
				SoundEffect(SFX_TR4_POUR_WATER, &laraItem->Pose);
				item->Pose.Orientation.y = rotY;
			}
			else if (laraItem->Animation.FrameNumber == 74)
			{
				AddActiveItem(itemNumber);
				item->Status = ITEM_ACTIVE;

				if (laraItem->ItemFlags[3] < item->TriggerFlags)
				{
					item->Animation.TargetState = 4;
					item->Pose.Orientation.y = rotY;
				}
				else if (laraItem->ItemFlags[3] == item->TriggerFlags)
				{
					item->Animation.TargetState = 2;
					item->Pose.Orientation.y = rotY;
				}
				else
					item->Animation.TargetState = 3;
			}
			else
				item->Pose.Orientation.y = rotY;
		}
	}
	
	if ((laraItem->Animation.FrameNumber >= 44 &&
		laraItem->Animation.FrameNumber <= 72) ||
		(laraItem->Animation.FrameNumber >= 51 &&
		laraItem->Animation.FrameNumber <= 74))
	{
		auto pos = GetJointPosition(laraItem, LM_LHAND).ToVector3();
		auto velocity = Vector3(0.0f, Random::GenerateFloat(32.0f, 64.0f), 0.0f);
		auto color = Vector4::One;
		float life = Random::GenerateFloat(16.0f, 48.0f);
		float gravity = Random::GenerateFloat(32.0f, 64.0f);

		SpawnDrip(pos, laraItem->RoomNumber, velocity, life, gravity);

		// TODO: Generate colours.
		/*drip->r = Random::GenerateFloat(24, 40);
		drip->g = Random::GenerateFloat(24, 60);
		drip->b = Random::GenerateFloat(24, 72);*/
	}
}
