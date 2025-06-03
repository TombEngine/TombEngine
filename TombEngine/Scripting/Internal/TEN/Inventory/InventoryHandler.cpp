#include "framework.h"
#include "Scripting/Internal/TEN/Inventory/InventoryHandler.h"

#include "Game/gui.h"
#include "Game/Hud/Hud.h"
#include "Game/Lara/lara.h"
#include "Game/pickup/pickup.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"

using namespace TEN::Hud;
using namespace TEN::Gui;

/***
Inventory manipulation
@tentable Inventory 
@pragma nostrip
*/

namespace TEN::Scripting::InventoryHandler
{
	/// Add an item to the player's inventory.
	//@function GiveItem
	//@tparam Objects.ObjID objectID Object ID of the item to add.
	//@tparam[opt=1] int count The amount of items to add. Default is the yield from a single pickup, e.g. 1 from a medipack, 12 from a flare pack.
	//@tparam[opt=false] bool addToPickupSummary If true, display the item in the pickup summary. Default is false.
	static void GiveItem(GAME_OBJECT_ID objectID, TypeOrNil<int> count, TypeOrNil<bool> addToPickupSummary)
	{
		auto convertedCount = ValueOr<int>(count, 1);

		PickedUpObject(objectID, convertedCount);
		
		if (ValueOr<bool>(addToPickupSummary, false))
		{
			constexpr auto START_POS_MULT = Vector2(1.1f, 0.85f);
			g_Hud.PickupSummary.AddDisplayPickup(objectID, DISPLAY_SPACE_RES * START_POS_MULT, convertedCount);
		}
	}

	/// Remove an item from the player's inventory.
	//@function TakeItem
	//@tparam Objects.ObjID Object ID of the item to remove.
	//@tparam[opt=1] int count The amount of items to remove. Default is the yield from a single pickup, e.g. 1 from a medipack, 12 from a flare pack.
	static void TakeItem(GAME_OBJECT_ID objectID, TypeOrNil<int> count)
	{
		auto convertedCount = ValueOr<int>(count, 1);
		RemoveObjectFromInventory(objectID, convertedCount);
	}

	/// Get the amount of an item held in the player's inventory.
	//@function GetItemCount
	//@tparam Objects.ObjID objectID Object ID of the item to check.
	//@treturn int The amount of items. -1 indicates infinity.
	static int GetItemCount(GAME_OBJECT_ID objectID)
	{
		return GetInventoryCount(objectID);
	}

	/// Set the amount of an item in the player's inventory.
	//@function SetItemCount
	//@tparam Objects.ObjID objectID Object ID of the item amount to set.
	//@tparam int count The amount of items to set. -1 indicates infinity.
	static void SetItemCount(GAME_OBJECT_ID objectID, int count)
	{
		SetInventoryCount(objectID, count);
	}

	/// Get last item used in the player's inventory.
	// This value will be valid only for a single frame after exiting inventory, after which Lara says "No".
	// Therefore, this function must be preferably used either in OnLoop or OnUseItem events.
	//@function GetUsedItem
	//@treturn Objects.ObjID Last item used in the inventory.
	static GAME_OBJECT_ID GetUsedItem()
	{
		return (GAME_OBJECT_ID)g_Gui.GetInventoryItemChosen();
	}

	/// Set last item used in the player's inventory.
	// You will be able to specify only objects which already exist in the inventory.
	// Will only be valid for the next frame. If not processed by the game, Lara will say "No".
	//@function SetUsedItem
	//@tparam Objects.ObjID objectID Object ID of the item to select from inventory.
	static void SetUsedItem(GAME_OBJECT_ID objectID)
	{
		if (g_Gui.IsObjectInInventory(objectID))
			g_Gui.SetInventoryItemChosen(objectID);
	}

	/// Clear last item used in the player's inventory.
	// When this function is used in OnUseItem level function, it allows to override existing item functionality.
	// For items without existing functionality, this function is needed to avoid Lara saying "No" after using it.
	//@function ClearUsedItem
	static void ClearUsedItem()
	{
		g_Gui.SetInventoryItemChosen(GAME_OBJECT_ID::ID_NO_OBJECT);
	}

	void Register(sol::state* state, sol::table& parent)
	{
		auto tableInventory = sol::table{ state->lua_state(), sol::create };
		parent.set(ScriptReserved_Inventory, tableInventory);

		tableInventory.set_function(ScriptReserved_GiveInvItem, &GiveItem);
		tableInventory.set_function(ScriptReserved_TakeInvItem, &TakeItem);
		tableInventory.set_function(ScriptReserved_GetInvItemCount, &GetItemCount);
		tableInventory.set_function(ScriptReserved_SetInvItemCount, &SetItemCount);
		tableInventory.set_function(ScriptReserved_SetUsedItem, &SetUsedItem);
		tableInventory.set_function(ScriptReserved_GetUsedItem, &GetUsedItem);
		tableInventory.set_function(ScriptReserved_ClearUsedItem, &ClearUsedItem);
	}
}
