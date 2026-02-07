#pragma once

#include "Game/Lara/lara_struct.h"

/// Constants for Weapon modes.
// To be used with @{Objects.LaraObject.SetWeaponMode} function. Currently only works for HK Weapon.
// @enum Objects.WeaponMode
// @pragma nostrip

namespace TEN::Scripting
{

	static const auto WEAPON_MODES = std::unordered_map<std::string, PlayerWeaponMode>
	{
		/// Rapid fire mode.
		// @mem RAPID
		{ "RAPID", PlayerWeaponMode::Rapid },

		/// Burst fire mode.
		// @mem BURST
		{ "BURST", PlayerWeaponMode::Burst },

		/// Sniper mode.
		// @mem SNIPER
		{ "SNIPER", PlayerWeaponMode::Sniper }
	};
}
