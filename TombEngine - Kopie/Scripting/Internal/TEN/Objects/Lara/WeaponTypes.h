#pragma once

#include "Game/control/control.h"
#include "Game/Lara/lara_struct.h"

namespace TEN::Scripting
{
	/// Constants for weapon types.
	// To be used with @{Objects.LaraObject.GetWeaponType} and @{Objects.LaraObject.SetWeaponType} functions.
	// <br>
	// Note that this table also contains the flare and torch, as they are internally counted as "weapons" in the engine.
	// @enum Objects.WeaponType
	// @pragma nostrip

	static const auto WEAPON_TYPES = std::unordered_map<std::string, LaraWeaponType>
	{
		/// No weapon.
		// @mem NONE
		{ "NONE", LaraWeaponType::None },

		/// Pistols.
		// @mem PISTOLS
		{ "PISTOLS", LaraWeaponType::Pistol },

		/// Uzi submachine guns.
		// @mem UZIS
		{ "UZIS", LaraWeaponType::Uzi },

		/// Revolver.
		// @mem REVOLVER
		{ "REVOLVER", LaraWeaponType::Revolver },

		/// Shotgun.
		// @mem SHOTGUN
		{ "SHOTGUN", LaraWeaponType::Shotgun },

		/// HK MP5.
		// @mem HK
		{ "HK", LaraWeaponType::HK },

		/// Crossbow.
		// @mem CROSSBOW
		{ "CROSSBOW", LaraWeaponType::Crossbow },

		/// Flare.
		// @mem FLARE
		{ "FLARE", LaraWeaponType::Flare },

		/// Torch.
		// @mem TORCH
		{ "TORCH", LaraWeaponType::Torch },

		/// Grenade launcher.
		// @mem GRENADE_LAUNCHER
		{ "GRENADE_LAUNCHER", LaraWeaponType::GrenadeLauncher },

		/// Harpoon gun.
		// @mem HARPOON_GUN
		{ "HARPOON_GUN", LaraWeaponType::HarpoonGun },

		/// Rocket launcher.
		// @mem ROCKET_LAUNCHER
		{ "ROCKET_LAUNCHER", LaraWeaponType::RocketLauncher },

		// COMPATIBILITY
		{ "PISTOL", LaraWeaponType::Pistol },
		{ "UZI", LaraWeaponType::Uzi }
	};
}
