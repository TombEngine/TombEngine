#pragma once

#include "framework.h"
#include "Game/GuiObjects.h"

using namespace TEN::Gui;

namespace TEN::Scripting
{
	/// Constants for item actions.
	// @enum Flow.ItemAction
	// @pragma nostrip

	/// Table of Flow.ItemAction constants. To be used with @{Flow.InventoryItem} class.
	//
	// - `USE` - Use the item.
	// - `EQUIP` - Equip the item. Used only with weapons.
	// - `EXAMINE` - Examine the item. Used only with examine objects.
	// - `COMBINE` - Combine the item with another item. Used only with combineable objects.
	// - `SEPARATE` - Separate items. Used only with objects that were combined before.
	// - `LOAD` - Load game.
	// - `SAVE` - Save game.
	// - `STATISTICS` - Show statistics screen.
	// - `CHOOSE_AMMO_SHOTGUN` - Choose ammo type for the shotgun.
	// - `CHOOSE_AMMO_CROSSBOW` - Choose ammo type for the crossbow.
	// - `CHOOSE_AMMO_GRENADEGUN` - Choose ammo type for the grenade launcher.
	// - `CHOOSE_AMMO_UZI` - Choose ammo type for the Uzi.
	// - `CHOOSE_AMMO_PISTOLS` - Choose ammo type for the pistols.
	// - `CHOOSE_AMMO_REVOLVER` - Choose ammo type for the revolver.
	// - `CHOOSE_AMMO_HK` - Choose ammo type for the HK gun.
	// - `CHOOSE_AMMO_HARPOON` - Choose ammo type for the harpoon gun.
	// - `CHOOSE_AMMO_ROCKET` - Choose ammo type for the rocket launcher.
	//
	// @table Flow.ItemAction

	static const auto ITEM_MENU_ACTIONS = std::unordered_map<std::string, ItemOptions>
	{
		{ "USE", ItemOptions::OPT_USE },
		{ "EQUIP", ItemOptions::OPT_EQUIP },
		{ "EXAMINE", ItemOptions::OPT_EXAMINABLE },
		{ "COMBINE", ItemOptions::OPT_COMBINABLE },
		{ "SEPARATE", ItemOptions::OPT_SEPARABLE },
		{ "LOAD", ItemOptions::OPT_LOAD },
		{ "SAVE", ItemOptions::OPT_SAVE },
		{ "STATISTICS", ItemOptions::OPT_STATS },

		{ "CHOOSE_AMMO_SHOTGUN", ItemOptions::OPT_CHOOSE_AMMO_SHOTGUN },
		{ "CHOOSE_AMMO_CROSSBOW", ItemOptions::OPT_CHOOSE_AMMO_CROSSBOW },
		{ "CHOOSE_AMMO_GRENADEGUN", ItemOptions::OPT_CHOOSE_AMMO_GRENADEGUN },
		{ "CHOOSE_AMMO_UZI", ItemOptions::OPT_CHOOSE_AMMO_UZI },
		{ "CHOOSE_AMMO_PISTOLS", ItemOptions::OPT_CHOOSE_AMMO_PISTOLS },
		{ "CHOOSE_AMMO_REVOLVER", ItemOptions::OPT_CHOOSE_AMMO_REVOLVER },
		{ "CHOOSE_AMMO_HK", ItemOptions::OPT_CHOOSE_AMMO_HK },
		{ "CHOOSE_AMMO_HARPOON", ItemOptions::OPT_CHOOSE_AMMO_HARPOON },
		{ "CHOOSE_AMMO_ROCKET", ItemOptions::OPT_CHOOSE_AMMO_ROCKET },
	};
}
