#pragma once

#include "Game/Lara/lara_struct.h"

namespace TEN::Scripting
{
	/// Constants for player hand statuses.
	// To be used with @{Objects.LaraObject.GetAmmoType} function.
	// @enum Objects.HandStatus
	// @pragma nostrip

	static const auto HAND_STATUSES = std::unordered_map<std::string, HandStatus>
	{
		/// Hand is free.
		// @mem FREE
		{ "FREE", HandStatus::Free },

		/// Hand is busy.
		// @mem BUSY
		{ "BUSY", HandStatus::Busy },

		/// Hand is drawing weapon.
		// @mem WEAPON_DRAW
		{ "WEAPON_DRAW", HandStatus::WeaponDraw },

		/// Hand is undrawing weapon.
		// @mem WEAPON_UNDRAW
		{ "WEAPON_UNDRAW", HandStatus::WeaponUndraw },

		/// Hand has weapon ready.
		// @mem WEAPON_READY
		{ "WEAPON_READY", HandStatus::WeaponReady },

		/// Hand is in special state.
		// @mem SPECIAL
		{ "SPECIAL", HandStatus::Special },

		// COMPATIBILITY
		{ "WEAPONDRAW", HandStatus::WeaponDraw },
		{ "WEAPONUNDRAW", HandStatus::WeaponUndraw },
		{ "WEAPONREADY", HandStatus::WeaponReady }
	};
}
