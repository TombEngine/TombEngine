#include "framework.h"
#include "tr4_element_puzzle.h"

#include "Specific/level.h"
#include "Game/Collision/Sphere.h"
#include "Game/control/control.h"
#include "Sound/sound.h"
#include "Game/Animation/Animation.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Specific/Input/Input.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/collide_item.h"
#include "Game/items.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;
using namespace TEN::Input;
using namespace TEN::Entities::Switches;

namespace TEN::Entities::TR4
{
	ObjectCollisionBounds ElementPuzzleBounds =
	{
		GameBoundingBox(
			0, 0,
			-CLICK(0.25f), 0,
			0, 0
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};

	void ElementPuzzleControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (item->TriggerFlags == 1)
		{
			SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &item->Pose);

			byte r = (GetRandomControl() & 0x3F) + 192;
			byte g = (GetRandomControl() & 0x1F) + 96;
			byte b = 0;
			short fade = 0;

			if (item->ItemFlags[3])
			{
				item->ItemFlags[3]--;
				fade = 255 - GetRandomControl() % (4 * (91 - item->ItemFlags[3]));
				if (fade < 1)
				{
					fade = 1;
					r = (r * fade) / 256;
					g = (g * fade) / 256;
				}
				else if (fade <= 255)
				{
					r = (r * fade) / 256;
					g = (g * fade) / 256;
				}
			}
			else
				fade = 0;

			AddFire(item->Pose.Position.x, item->Pose.Position.y - 620, item->Pose.Position.z, item->RoomNumber, 0.5f, fade);
			SpawnDynamicLight(item->Pose.Position.x, item->Pose.Position.y - 768, item->Pose.Position.z, 12, r, g, b);
			return;
		}

		if (item->TriggerFlags != 3)
			return;

		if (item->ItemFlags[1] > 90)
			SoundEffect(SFX_TR4_WIND, &item->Pose);

		if (item->ItemFlags[1] < 60)
		{
			item->ItemFlags[1]++;
			return;
		}

		item->ItemFlags[0]++;

		if (item->ItemFlags[0] == 90)
		{
			short itemNos[256];
			int sw = GetSwitchTrigger(item, itemNos, 0);
			if (sw > 0)
			{
				for (int i = 0; i < sw; i++)
				{
					AddActiveItem(itemNos[i]);
					g_Level.Items[itemNos[i]].Status = ITEM_ACTIVE;
					g_Level.Items[itemNos[i]].Flags |= CODE_BITS;
				}
			}

			KillItem(itemNumber);
			return;
		}

		short currentItemNumber = g_Level.Rooms[item->RoomNumber].itemNumber;
		if (currentItemNumber == NO_VALUE)
			return;

		while (currentItemNumber != NO_VALUE)
		{
			auto* currentItem = &g_Level.Items[currentItemNumber];

			if (currentItem->ObjectNumber != ID_FLAME_EMITTER2)
			{
				if (currentItem->ObjectNumber == ID_ELEMENT_PUZZLE &&
					currentItem->TriggerFlags == 1 &&
					!currentItem->ItemFlags[3])
				{
					currentItem->ItemFlags[3] = 90;
				}

				currentItemNumber = currentItem->NextItem;
				continue;
			}

			if (item->ItemFlags[0] != 89)
			{
				currentItem->ItemFlags[3] = 255 - GetRandomControl() % (4 * item->ItemFlags[0]);
				if (currentItem->ItemFlags[3] >= 2)
				{
					currentItemNumber = currentItem->NextItem;
					continue;
				}

				currentItem->ItemFlags[3] = 2;
			}

			RemoveActiveItem(currentItemNumber);
			currentItem->Status = ITEM_NOT_ACTIVE;
		}
	}

	void ElementPuzzleDoCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (TestBoundsCollide(item, laraItem, coll->Setup.Radius))
		{
			if (HandleItemSphereCollision(*item, *laraItem))
			{
				if (coll->Setup.EnableObjectPush)
					ItemPushItem(item, laraItem, coll, false, 0);
			}
		}
	}

	void ElementPuzzleCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* puzzleItem = &g_Level.Items[itemNumber];

		int flags = 0;

		if (puzzleItem->TriggerFlags)
		{
			if (puzzleItem->TriggerFlags == 1)
				flags = 26;
			else
			{
				if (puzzleItem->TriggerFlags != 2)
					return;
			 
				flags = 27;
			}
		}
		else
			flags = 25;

		if ((laraItem->Animation.AnimNumber == LA_WATERSKIN_POUR_LOW ||
			laraItem->Animation.AnimNumber == LA_WATERSKIN_POUR_HIGH) &&
			!puzzleItem->ItemFlags[0])
		{
			auto box = GameBoundingBox(puzzleItem);

			ElementPuzzleBounds.BoundingBox.X1 = box.X1;
			ElementPuzzleBounds.BoundingBox.X2 = box.X2;
			ElementPuzzleBounds.BoundingBox.Z1 = box.Z1 - 200;
			ElementPuzzleBounds.BoundingBox.Z2 = box.Z2 + 200;

			short oldRot = puzzleItem->Pose.Orientation.y;
			puzzleItem->Pose.Orientation.y = laraItem->Pose.Orientation.y;

			if (TestLaraPosition(ElementPuzzleBounds, puzzleItem, laraItem))
			{
				if (laraItem->Animation.AnimNumber == LA_WATERSKIN_POUR_LOW && LaraItem->ItemFlags[2] == flags)
				{
					laraItem->Animation.AnimNumber = LA_WATERSKIN_POUR_HIGH;
					laraItem->Animation.FrameNumber = 0;
				}

				if (laraItem->Animation.FrameNumber == 74 &&
					LaraItem->ItemFlags[2] == flags)
				{
					if (!puzzleItem->TriggerFlags)
					{
						puzzleItem->MeshBits = 48;
						TestTriggers(puzzleItem, true, puzzleItem->Flags & IFLAG_ACTIVATION_MASK);
						puzzleItem->ItemFlags[0] = 1;
						puzzleItem->Pose.Orientation.y = oldRot;
						return;
					}

					if (puzzleItem->TriggerFlags == 1)
					{
						puzzleItem->MeshBits = 3;
						laraInfo->Inventory.Pickups[1]--;
						puzzleItem->ItemFlags[0] = 1;
						puzzleItem->Pose.Orientation.y = oldRot;
						return;
					}

					puzzleItem->MeshBits = 12;
					TestTriggers(puzzleItem, true, puzzleItem->Flags & IFLAG_ACTIVATION_MASK);
					laraInfo->Inventory.Pickups[0]--;
					puzzleItem->ItemFlags[0] = 1;
				}
			}

			puzzleItem->Pose.Orientation.y = oldRot;
		}
		else
		{
			if (laraInfo->Control.Weapon.GunType != LaraWeaponType::Torch ||
				laraInfo->Control.HandStatus != HandStatus::WeaponReady ||
				laraInfo->LeftArm.Locked ||
				!IsHeld(In::Action) ||
				puzzleItem->TriggerFlags != 1 ||
				puzzleItem->ItemFlags[0] != 1 ||
				laraItem->Animation.ActiveState != LS_IDLE ||
				laraItem->Animation.AnimNumber != LA_STAND_IDLE ||
				!laraInfo->Torch.IsLit ||
				laraItem->Animation.IsAirborne)
			{
				if (laraItem->Animation.AnimNumber != LA_TORCH_LIGHT_3 ||
					16 || // TODO: Typo?
					puzzleItem->ItemFlags[0] != 2)
				{
					ElementPuzzleDoCollision(itemNumber, laraItem, coll);
				}
				else
				{
					TestTriggers(puzzleItem, true, puzzleItem->Flags & IFLAG_ACTIVATION_MASK);
					AddActiveItem(itemNumber);
					puzzleItem->Status = ITEM_ACTIVE;
					puzzleItem->ItemFlags[0] = 3;
					puzzleItem->Flags |= CODE_BITS;
				}
			}
			else
			{
				auto box = GameBoundingBox(puzzleItem);

				ElementPuzzleBounds.BoundingBox.X1 = box.X1;
				ElementPuzzleBounds.BoundingBox.X2 = box.X2;
				ElementPuzzleBounds.BoundingBox.Z1 = box.Z1 - 200;
				ElementPuzzleBounds.BoundingBox.Z2 = box.Z2 + 200;

				short oldRot = puzzleItem->Pose.Orientation.y;
				puzzleItem->Pose.Orientation.y = laraItem->Pose.Orientation.y;

				if (TestLaraPosition(ElementPuzzleBounds, puzzleItem, laraItem))
				{
					laraItem->Animation.AnimNumber = (abs(puzzleItem->Pose.Position.y - laraItem->Pose.Position.y) >> 8) + LA_TORCH_LIGHT_3;
					laraItem->Animation.FrameNumber = 0;
					laraItem->Animation.ActiveState = LS_MISC_CONTROL;
					laraInfo->Flare.ControlLeft = false;
					laraInfo->LeftArm.Locked = true;
					puzzleItem->ItemFlags[0] = 2;
				}

				puzzleItem->Pose.Orientation.y = oldRot;
			}
		}
	}

	void InitializeElementPuzzle(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->TriggerFlags)
		{
			if (item->TriggerFlags == 1)
				item->MeshBits = 65;
			else if (item->TriggerFlags == 2)
				item->MeshBits = 68;
			else
				item->MeshBits = 0;
		}
		else
			item->MeshBits = 80;
	}
}
