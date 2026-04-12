#include "framework.h"
#include "tr4_element_puzzle.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;
using namespace TEN::Gui;
using namespace TEN::Hud;
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

			AddFire(item->Pose.Position.x, item->Pose.Position.y - 620, item->Pose.Position.z, item->RoomNumber, 1.0f, fade);
			SpawnDynamicLight(item->Pose.Position.x, item->Pose.Position.y - 768, item->Pose.Position.z, 12, r, g, b);
			return;
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

	// Returns the object ID of the first filled waterskin in inventory, or NO_VALUE if none.
	static GAME_OBJECT_ID GetFilledWaterskinID(const LaraInfo& player)
	{
		if (player.Inventory.SmallWaterskin > 1)
			return GAME_OBJECT_ID(ID_WATERSKIN1_EMPTY + player.Inventory.SmallWaterskin - 1);

		if (player.Inventory.BigWaterskin > 1)
			return GAME_OBJECT_ID(ID_WATERSKIN2_EMPTY + player.Inventory.BigWaterskin - 1);

		return (GAME_OBJECT_ID)NO_VALUE;
	}

	void ElementPuzzleCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* puzzleItem = &g_Level.Items[itemNumber];

		// Suppress the highlighter once fully done (water/earth = 1, fire torch phase = 2, fire lit = 3).
		// Fire still needs the torch interaction at ItemFlags[0] == 1, so only early-out for non-fire
		// or for fire once it's past the torch-lighting phase.
		if (puzzleItem->ItemFlags[0] &&
			!(puzzleItem->TriggerFlags == 1 && puzzleItem->ItemFlags[0] <= 2))
		{
			ElementPuzzleDoCollision(itemNumber, laraItem, coll);
			return;
		}

		g_Hud.InteractionHighlighter.Test(*laraItem, *puzzleItem);

		// Determine which mesh-swap flag value this element expects on LaraItem->ItemFlags[2].
		// 0 = water  → ID_LARA_WATER_MESH  (25)
		// 1 = fire   → ID_LARA_PETROL_MESH (26)
		// 2 = earth  → ID_LARA_DIRT_MESH   (27)
		int expectedMeshFlag = 0;
		if (puzzleItem->TriggerFlags == 0) 
			expectedMeshFlag = ID_LARA_WATER_MESH;
		else if (puzzleItem->TriggerFlags == 1) 
			expectedMeshFlag = ID_LARA_PETROL_MESH;
		else if (puzzleItem->TriggerFlags == 2) 
			expectedMeshFlag = ID_LARA_DIRT_MESH;
		else 
		{ 
			ElementPuzzleDoCollision(itemNumber, laraItem, coll); 
			return; 
		}

		// ----------------------------------------------------------------
		// Water / Fire / Earth pour animation in progress — handle frames
		// ----------------------------------------------------------------
		if ((laraItem->Animation.AnimNumber == LA_WATERSKIN_POUR_LOW ||
			laraItem->Animation.AnimNumber == LA_WATERSKIN_POUR_HIGH) &&
			LaraItem->ItemFlags[2] == expectedMeshFlag)
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
				// Upgrade low pour to high pour.
				if (laraItem->Animation.AnimNumber == LA_WATERSKIN_POUR_LOW)
				{
					laraItem->Animation.AnimNumber = LA_WATERSKIN_POUR_HIGH;
					laraItem->Animation.FrameNumber = 0;
				}

				// Completion frame.
				if (laraItem->Animation.FrameNumber == 74)
				{
					if (puzzleItem->TriggerFlags == 0) // Water
					{
						puzzleItem->MeshBits = 48;
						TestTriggers(puzzleItem, true, puzzleItem->Flags & IFLAG_ACTIVATION_MASK);
					}
					else if (puzzleItem->TriggerFlags == 1) // Fire — torch will ignite it later
					{
						puzzleItem->MeshBits = 3;
						laraInfo->Inventory.Pickups[ID_PICKUP_ITEM2 - ID_PICKUP_ITEM1]--;
					}
					else // Earth
					{
						puzzleItem->MeshBits = 12;
						TestTriggers(puzzleItem, true, puzzleItem->Flags & IFLAG_ACTIVATION_MASK);
						laraInfo->Inventory.Pickups[ID_PICKUP_ITEM1 - ID_PICKUP_ITEM1]--;
					}

					puzzleItem->ItemFlags[0] = 1;
				}
			}

			puzzleItem->Pose.Orientation.y = oldRot;
			return;
		}

		// ----------------------------------------------------------------
		// Fire element, step 2: light it with the torch
		// ----------------------------------------------------------------
		if (puzzleItem->TriggerFlags == 1 && puzzleItem->ItemFlags[0] >= 1 && puzzleItem->ItemFlags[0] <= 2)
		{
			// Torch-lighting animation completion.
			if (laraItem->Animation.AnimNumber == LA_TORCH_LIGHT_3 &&
				laraItem->Animation.FrameNumber == 16 &&
				puzzleItem->ItemFlags[0] == 2)
			{
				TestTriggers(puzzleItem, true, puzzleItem->Flags & IFLAG_ACTIVATION_MASK);
				AddActiveItem(itemNumber);
				puzzleItem->Status = ITEM_ACTIVE;
				puzzleItem->ItemFlags[0] = 3;
				puzzleItem->Flags |= CODE_BITS;
				return;
			}

			// Player is ready to light: bearing a lit torch, standing still, action pressed.
			if (laraInfo->Control.Weapon.GunType == LaraWeaponType::Torch &&
				laraInfo->Control.HandStatus == HandStatus::WeaponReady &&
				!laraInfo->LeftArm.Locked &&
				IsHeld(In::Action) &&
				laraItem->Animation.ActiveState == LS_IDLE &&
				laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
				laraInfo->Torch.IsLit &&
				!laraItem->Animation.IsAirborne)
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
					laraItem->Animation.AnimNumber = LA_TORCH_LIGHT_3;
					laraItem->Animation.FrameNumber = 0;
					laraItem->Animation.ActiveState = LS_MISC_CONTROL;
					puzzleItem->ItemFlags[0] = 2;
				}

				puzzleItem->Pose.Orientation.y = oldRot;
				return;
			}

			ElementPuzzleDoCollision(itemNumber, laraItem, coll);
			return;
		}

		// ----------------------------------------------------------------
		// Check proximity and open the inventory to the correct item
		// ----------------------------------------------------------------
		bool isPlayerIdle = (laraItem->Animation.ActiveState == LS_IDLE &&
			laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
			laraInfo->Control.HandStatus == HandStatus::Free);

		if ((IsHeld(In::Action) || g_Gui.GetInventoryItemChosen() != NO_VALUE) && isPlayerIdle)
		{
			auto box = GameBoundingBox(puzzleItem);
			ElementPuzzleBounds.BoundingBox.X1 = box.X1;
			ElementPuzzleBounds.BoundingBox.X2 = box.X2;
			ElementPuzzleBounds.BoundingBox.Z1 = box.Z1 - 200;
			ElementPuzzleBounds.BoundingBox.Z2 = box.Z2 + 200;

			short oldRot = puzzleItem->Pose.Orientation.y;
			puzzleItem->Pose.Orientation.y = laraItem->Pose.Orientation.y;
			bool inRange = TestLaraPosition(ElementPuzzleBounds, puzzleItem, laraItem);
			puzzleItem->Pose.Orientation.y = oldRot;

			if (inRange)
			{
				int chosen = g_Gui.GetInventoryItemChosen();

				if (chosen == NO_VALUE)
				{
					// Open inventory to the appropriate item.
					if (puzzleItem->TriggerFlags == 0) // Water
					{
						GAME_OBJECT_ID filledSkin = GetFilledWaterskinID(*laraInfo);
						if (filledSkin != (GAME_OBJECT_ID)NO_VALUE)
							g_Gui.SetEnterInventory(filledSkin);
						else if (IsClicked(In::Action))
							SayNo();
					}
					else if (puzzleItem->TriggerFlags == 1) // Fire
					{
						if (g_Gui.IsObjectInInventory(ID_PICKUP_ITEM2))
							g_Gui.SetEnterInventory(ID_PICKUP_ITEM2);
						else if (IsClicked(In::Action))
							SayNo();
					}
					else // Earth
					{
						if (g_Gui.IsObjectInInventory(ID_PICKUP_ITEM1))
							g_Gui.SetEnterInventory(ID_PICKUP_ITEM1);
						else if (IsClicked(In::Action))
							SayNo();
					}

					return;
				}

				// Validate that the chosen item is correct for this element.
				bool validChoice = false;
				if (puzzleItem->TriggerFlags == 0)
					validChoice = (chosen >= ID_WATERSKIN1_1 && chosen <= ID_WATERSKIN1_3) ||
					              (chosen >= ID_WATERSKIN2_1 && chosen <= ID_WATERSKIN2_5);
				else if (puzzleItem->TriggerFlags == 1)
					validChoice = (chosen == ID_PICKUP_ITEM2);
				else
					validChoice = (chosen == ID_PICKUP_ITEM1);

				g_Gui.SetInventoryItemChosen(NO_VALUE);

				if (!validChoice)
					return;

				// Set the mesh-swap flag so the pour animation acts on the right element.
				LaraItem->ItemFlags[2] = expectedMeshFlag;
				laraItem->Animation.AnimNumber = LA_WATERSKIN_POUR_HIGH;
				laraItem->Animation.ActiveState = LS_MISC_CONTROL;
				laraItem->Animation.FrameNumber = 0;
				return;
			}
		}

		ElementPuzzleDoCollision(itemNumber, laraItem, coll);
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
