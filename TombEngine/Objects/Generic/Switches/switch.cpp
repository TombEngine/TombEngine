#include "framework.h"
#include "Objects/Generic/Switches/switch.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/lot.h"
#include "Game/effects/debris.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/objects.h"
#include "Specific/level.h"
#include "Specific/Input/Input.h"
#include "Sound/sound.h"

using namespace TEN::Animation;

// NOTE: we need to decompile/inspect if these functions are still needed

void ProcessShootSwitch(ItemInfo* item) 
{
	if (item->Flags & IFLAG_SWITCH_ONESHOT)
		return;

	if (item->ObjectNumber == ID_SHOOT_SWITCH1 || item->ObjectNumber == ID_SHOOT_SWITCH2)
		ExplodeItemNode(item, Objects[item->ObjectNumber].nmeshes - 1, 0, 64);

	if (item->ObjectNumber == ID_SHOOT_SWITCH2 && item->TriggerFlags == 444)
	{
		auto pos = GetJointPosition(item, 0);
		TestTriggers(pos.x, pos.y, pos.z, item->RoomNumber, true);
		item->MeshBits |= 1 << ((Objects[item->ObjectNumber].nmeshes & 0xFF) - 2);
	}
	else
	{
		if (item->Flags & IFLAG_ACTIVATION_MASK && (item->Flags & IFLAG_ACTIVATION_MASK) != IFLAG_ACTIVATION_MASK)
		{
			TestTriggers(item->Pose.Position.x, item->Pose.Position.y - 256, item->Pose.Position.z, item->RoomNumber, true, item->Flags & IFLAG_ACTIVATION_MASK);
		}
		else
		{
			short triggerItems[8];
			for (int count = GetSwitchTrigger(item, triggerItems, 1); count > 0; --count)
			{
				AddActiveItem(triggerItems[count - 1]);
				g_Level.Items[triggerItems[count - 1]].Status = ITEM_ACTIVE;
				g_Level.Items[triggerItems[count - 1]].Flags |= IFLAG_ACTIVATION_MASK;
			}
		}
	}

	if (item->Status != ITEM_DEACTIVATED)
	{
		AddActiveItem(item->Index);
		item->Status = ITEM_ACTIVE;
		item->Flags |= IFLAG_ACTIVATION_MASK | IFLAG_SWITCH_ONESHOT;
	}
}

void InitializeShootSwitch(short itemNumber)
{
	auto* switchItem = &g_Level.Items[itemNumber];

	if (switchItem->TriggerFlags == 444)
		switchItem->MeshBits &= ~(1 << (Objects[switchItem->ObjectNumber].nmeshes - 2));
}

void ShootSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	auto* switchItem = &g_Level.Items[itemNumber];

	if (switchItem->ObjectNumber == ID_SHOOT_SWITCH1 && !(switchItem->MeshBits & 1))
		switchItem->Status = ITEM_INVISIBLE;
}
