#include "framework.h"
#include "Objects/TR4/Object/tr4_scales.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/control/control.h"
#include "Game/effects/Drip.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Objects/TR4/Entity/tr4_ahmet.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Objects/objectslist.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Drip;
using namespace TEN::Entities::Switches;
using namespace TEN::Entities::TR4;
using namespace TEN::Gui;
using namespace TEN::Hud;
using namespace TEN::Input;

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

	if (!TestLastFrame(*item))
	{
		AnimateItem(item);
		return;
	}

	if (item->Animation.ActiveState == 1 || item->ItemFlags[1])
	{
		if (item->Animation.AnimNumber == 0)
		{
			RemoveActiveItem(itemNumber);
			item->Status = ITEM_NOT_ACTIVE;
			item->ItemFlags[1] = 0;

			AnimateItem(item);
			return;
		}

		if (RespawnAhmet(Lara.Context.InteractedItem))
		{
			short itemNos[8];
			int sw = GetSwitchTrigger(item, itemNos, 0);

			for (int i = sw - 1; i >= 0; i--)
			{
				if (g_Level.Items[itemNos[i]].ObjectNumber != ID_FLAME_EMITTER2)
					g_Level.Items[itemNos[i]].Flags = 1024;
			}

			item->Animation.TargetState = 1;
		}

		AnimateItem(item);
		return;
	}

	if (item->Animation.ActiveState == 2)
	{
		// Correct water amount.
		RemoveActiveItem(itemNumber);
		item->Status = ITEM_NOT_ACTIVE;
		TestTriggers(item, true, -512);
	}
	else
	{
		// Wrong water amount.
		item->ItemFlags[1] = 1;
		TestTriggers(item, true, -1024);
	}
	AnimateItem(item);
}

void ScalesCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	auto* item = &g_Level.Items[itemNumber];
	auto* laraInfo = GetLaraInfo(laraItem);

	// Suppress highlighter once water has been poured (wrong: ItemFlags[1] = 1, correct: item deactivated).
	// It reappears when the scale resets (ItemFlags[1] cleared back to 0).
	bool waterPoured = (item->ItemFlags[1] != 0 || item->Status == ITEM_ACTIVE);

	if (!waterPoured)
		g_Hud.InteractionHighlighter.Test(*laraItem, *item);

	// Idle: open inventory to a filled waterskin, or handle the choice
	bool isPlayerIdle = (laraItem->Animation.ActiveState == LS_IDLE &&
		laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
		laraInfo->Control.HandStatus == HandStatus::Free);

	if (!waterPoured &&
		(IsHeld(In::Action) || g_Gui.GetInventoryItemChosen() != NO_VALUE) && isPlayerIdle &&
		TestBoundsCollide(item, laraItem, LARA_RADIUS))
	{
		int chosen = g_Gui.GetInventoryItemChosen();

		if (chosen == NO_VALUE)
		{
			// Open inventory to the first filled waterskin.
			GAME_OBJECT_ID filledSkin = (GAME_OBJECT_ID)NO_VALUE;
			if (laraInfo->Inventory.SmallWaterskin > 1)
				filledSkin = GAME_OBJECT_ID(ID_WATERSKIN1_EMPTY + laraInfo->Inventory.SmallWaterskin - 1);
			else if (laraInfo->Inventory.BigWaterskin > 1)
				filledSkin = GAME_OBJECT_ID(ID_WATERSKIN2_EMPTY + laraInfo->Inventory.BigWaterskin - 1);

			if (filledSkin != (GAME_OBJECT_ID)NO_VALUE)
				g_Gui.SetEnterInventory(filledSkin);
			else if (IsClicked(In::Action))
				SayNo();

			return;
		}

		// Validate that the chosen item is a filled waterskin.
		bool validChoice = (chosen >= ID_WATERSKIN1_1 && chosen <= ID_WATERSKIN1_3) ||
		                   (chosen >= ID_WATERSKIN2_1 && chosen <= ID_WATERSKIN2_5);

		g_Gui.SetInventoryItemChosen(NO_VALUE);

		if (!validChoice)
			return;

		// Consume waterskin and record water amount for scale comparison.
		int waterAmount = 0;
		if (chosen >= ID_WATERSKIN1_1 && chosen <= ID_WATERSKIN1_3)
		{
			waterAmount = laraInfo->Inventory.SmallWaterskin - 1;
			laraInfo->Inventory.SmallWaterskin = 1;
		}
		else
		{
			waterAmount = laraInfo->Inventory.BigWaterskin - 1;
			laraInfo->Inventory.BigWaterskin = 1;
		}

		laraItem->ItemFlags[3] = waterAmount;
		laraItem->Animation.AnimNumber = LA_WATERSKIN_POUR_HIGH;
		laraItem->Animation.FrameNumber = 0;
		laraItem->Animation.ActiveState = LS_MISC_CONTROL;
		item->Animation.ActiveState = 1;
		return;
	}

	if (TestBoundsCollide(item, laraItem, LARA_RADIUS))
	{
		if (laraItem->Animation.AnimNumber != LA_WATERSKIN_POUR_LOW &&
			laraItem->Animation.AnimNumber != LA_WATERSKIN_POUR_HIGH ||
			item->Animation.ActiveState != 1)
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
				if (laraItem->Animation.AnimNumber == LA_WATERSKIN_POUR_LOW)
				{
					laraItem->Animation.AnimNumber = LA_WATERSKIN_POUR_HIGH;
					laraItem->Animation.FrameNumber = 0;
				}
				else if (laraItem->Animation.FrameNumber == 51)
				{
					SoundEffect(SFX_TR4_POUR_WATER, &laraItem->Pose);
				}
				else if (laraItem->Animation.FrameNumber == 74)
				{
					AddActiveItem(itemNumber);
					item->Status = ITEM_ACTIVE;

					if (laraItem->ItemFlags[3] < item->TriggerFlags)
						item->Animation.TargetState = 4;
					else if (laraItem->ItemFlags[3] == item->TriggerFlags)
						item->Animation.TargetState = 2;
					else
						item->Animation.TargetState = 3;
				}
			}

			item->Pose.Orientation.y = rotY;
		}
	}

	if ((laraItem->Animation.AnimNumber == LA_WATERSKIN_POUR_LOW  && (laraItem->Animation.FrameNumber >= 44 && laraItem->Animation.FrameNumber <= 72)) ||
		(laraItem->Animation.AnimNumber == LA_WATERSKIN_POUR_HIGH && (laraItem->Animation.FrameNumber >= 51 && laraItem->Animation.FrameNumber <= 74)))
	{
		auto pos = GetJointPosition(laraItem, LM_LHAND).ToVector3();
		auto velocity = Vector3(0.0f, Random::GenerateFloat(32.0f, 64.0f), 0.0f);
		auto color = Vector4::One;
		float life = Random::GenerateFloat(16.0f, 48.0f);
		float gravity = Random::GenerateFloat(32.0f, 64.0f);

		SpawnDrip(pos, laraItem->RoomNumber, velocity, life, gravity);
	}
}
