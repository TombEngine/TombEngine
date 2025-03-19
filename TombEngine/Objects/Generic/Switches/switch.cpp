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

void ProcessExplodingSwitchType8(ItemInfo* item) 
{
	auto pos = GetJointPosition(item, 0);
	TestTriggers(pos.x, pos.y, pos.z, item->RoomNumber, true);

	ExplodeItemNode(item, Objects[item->ObjectNumber].nmeshes - 1, 0, 64);
	item->MeshBits |= 1 << ((Objects[item->ObjectNumber].nmeshes & 0xFF) - 2);
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
