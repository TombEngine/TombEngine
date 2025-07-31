#pragma once

#include "framework.h"
#include "Game/GuiObjects.h"

using namespace TEN::Gui;

namespace TEN::Scripting
{
	/// Constants for item actions.
	// To be used with @{Flow.InventoryItem} class.
	// @enum Flow.ItemAction
	// @pragma nostrip

	static const auto ITEM_MENU_ACTIONS = std::unordered_map<std::string, ItemOptions>
	{
		/// Use the item.
		// @mem USE
		{ "USE", ItemOptions::OPT_USE },

		/// Equip the item. Used only with weapons.
		// @mem EQUIP
		{ "EQUIP", ItemOptions::OPT_EQUIP },

		/// Examine the item. Used only with examine objects.
		// @mem EXAMINE
		{ "EXAMINE", ItemOptions::OPT_EXAMINABLE },

		/// Combine the item with another item. Used only with combineable objects.
		// @mem COMBINE
		{ "COMBINE", ItemOptions::OPT_COMBINABLE },

		/// Separate items. Used only with objects that were combined before.
		// @mem SEPARATE
		{ "SEPARATE", ItemOptions::OPT_SEPARABLE },

		/// Load game.
		// @mem LOAD
		{ "LOAD", ItemOptions::OPT_LOAD },

		/// Save game.
		// @mem SAVE
		{ "SAVE", ItemOptions::OPT_SAVE },

		/// Show statistics screen.
		// @mem STATISTICS
		{ "STATISTICS", ItemOptions::OPT_STATS },

		/// Choose ammo type for the shotgun.
		// @mem CHOOSE_AMMO_SHOTGUN
		{ "CHOOSE_AMMO_SHOTGUN", ItemOptions::OPT_CHOOSE_AMMO_SHOTGUN },

		/// Choose ammo type for the crossbow.
		// @mem CHOOSE_AMMO_CROSSBOW
		{ "CHOOSE_AMMO_CROSSBOW", ItemOptions::OPT_CHOOSE_AMMO_CROSSBOW },

		/// Choose ammo type for the grenade launcher.
		// @mem CHOOSE_AMMO_GRENADEGUN
		{ "CHOOSE_AMMO_GRENADEGUN", ItemOptions::OPT_CHOOSE_AMMO_GRENADEGUN },

		/// Choose ammo type for the Uzi.
		// @mem CHOOSE_AMMO_UZI
		{ "CHOOSE_AMMO_UZI", ItemOptions::OPT_CHOOSE_AMMO_UZI },

		/// Choose ammo type for the pistols.
		// @mem CHOOSE_AMMO_PISTOLS
		{ "CHOOSE_AMMO_PISTOLS", ItemOptions::OPT_CHOOSE_AMMO_PISTOLS },

		/// Choose ammo type for the revolver.
		// @mem CHOOSE_AMMO_REVOLVER
		{ "CHOOSE_AMMO_REVOLVER", ItemOptions::OPT_CHOOSE_AMMO_REVOLVER },

		/// Choose ammo type for the HK gun.
		// @mem CHOOSE_AMMO_HK
		{ "CHOOSE_AMMO_HK", ItemOptions::OPT_CHOOSE_AMMO_HK },

		/// Choose ammo type for the harpoon gun.
		// @mem CHOOSE_AMMO_HARPOON
		{ "CHOOSE_AMMO_HARPOON", ItemOptions::OPT_CHOOSE_AMMO_HARPOON },

		/// Choose ammo type for the rocket launcher.
		// @mem CHOOSE_AMMO_ROCKET
		{ "CHOOSE_AMMO_ROCKET", ItemOptions::OPT_CHOOSE_AMMO_ROCKET },
	};
}
