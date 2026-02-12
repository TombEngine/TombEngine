#pragma once

#include "Game/Lara/lara_struct.h"


/// Constants for Weapon modes.
// @enum Objects.WeaponMode
// @pragma nostrip

/// Table of Objects.WeaponMode constants.
// To be used with @{Objects.LaraObject.SetWeaponMode} function. Currently only works for HK Weapon.
//
// - `RAPID` Sets the Rapid mode.
// - `BURST` Sets the Burst mode.
// - `SNIPER` Sets the Sniper mode.
//
// @table Objects.WeaponMode
namespace TEN::Scripting
{

	static const auto WEAPON_MODES = std::unordered_map<std::string, PlayerWeaponMode>
	{
		{ "RAPID", PlayerWeaponMode::Rapid },
		{ "BURST", PlayerWeaponMode::Burst },
		{ "SNIPER", PlayerWeaponMode::Sniper }
	};
}
