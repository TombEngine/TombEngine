#pragma once

#include "Game/Lara/lara_struct.h"

/// Constants for player hand statuses.
// @enum Objects.HandStatus
// @pragma nostrip

/// Table of Objects.HandStatus constants.
// To be used with @{Objects.LaraObject.GetAmmoType} function.
//
// - `FREE`
// - `BUSY`
// - `WEAPON_DRAW`
// - `WEAPON_READY`
// - `SPECIAL`
//
// @table Objects.HandStatus

namespace TEN::Scripting
{
	static const auto HAND_STATUSES = std::unordered_map<std::string, HandStatus>
	{
		{ "FREE", HandStatus::Free },
		{ "BUSY", HandStatus::Busy },
		{ "WEAPON_DRAW", HandStatus::WeaponDraw },
		{ "WEAPON_UNDRAW", HandStatus::WeaponUndraw },
		{ "WEAPON_READY", HandStatus::WeaponReady },
		{ "SPECIAL", HandStatus::Special },

		// COMPATIBILITY
		{ "WEAPONDRAW", HandStatus::WeaponDraw },
		{ "WEAPONUNDRAW", HandStatus::WeaponUndraw },
		{ "WEAPONREADY", HandStatus::WeaponReady }
	};
}
