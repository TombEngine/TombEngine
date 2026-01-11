#pragma once

#include "Game/Lara/lara_struct.h"

namespace TEN::Scripting
{
	/// Constants for player weapon ammo types.
	// To be used with @{Objects.LaraObject.GetAmmoType} function.
	// @enum Objects.AmmoType
	// @pragma nostrip

	static const auto AMMO_TYPES = std::unordered_map<std::string, PlayerAmmoType>
	{
		/// Pistol ammo.
		// @mem PISTOLS
		{ "PISTOLS", PlayerAmmoType::Pistol },

		/// Revolver ammo.
		// @mem REVOLVER
		{ "REVOLVER", PlayerAmmoType::Revolver },

		/// Uzi ammo.
		// @mem UZI
		{ "UZI", PlayerAmmoType::Uzi },

		/// Normal shotgun shells.
		// @mem SHOTGUN_NORMAL
		{ "SHOTGUN_NORMAL", PlayerAmmoType::ShotgunNormal },

		/// Wide spread shotgun shells.
		// @mem SHOTGUN_WIDE
		{ "SHOTGUN_WIDE", PlayerAmmoType::ShotgunWide },

		/// HK ammo.
		// @mem HK
		{ "HK", PlayerAmmoType::HK },

		/// Normal crossbow bolts.
		// @mem CROSSBOW_BOLT_NORMAL
		{ "CROSSBOW_BOLT_NORMAL", PlayerAmmoType::CrossbowBoltNormal },

		/// Poison crossbow bolts.
		// @mem CROSSBOW_BOLT_POISON
		{ "CROSSBOW_BOLT_POISON", PlayerAmmoType::CrossbowBoltPoison },

		/// Explosive crossbow bolts.
		// @mem CROSSBOW_BOLT_EXPLOSIVE
		{ "CROSSBOW_BOLT_EXPLOSIVE", PlayerAmmoType::CrossbowBoltExplosive },

		/// Normal grenades.
		// @mem GRENADE_NORMAL
		{ "GRENADE_NORMAL", PlayerAmmoType::GrenadeNormal },

		/// Fragmentation grenades.
		// @mem GRENADE_FRAG
		{ "GRENADE_FRAG", PlayerAmmoType::GrenadeFrag },

		/// Flash grenades.
		// @mem GRENADE_FLASH
		{ "GRENADE_FLASH", PlayerAmmoType::GrenadeFlash },

		/// Harpoon ammunition.
		// @mem HARPOON
		{ "HARPOON", PlayerAmmoType::Harpoon },

		/// Rocket ammunition.
		// @mem ROCKET
		{ "ROCKET", PlayerAmmoType::Rocket }
	};
}
