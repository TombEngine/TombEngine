#pragma once

#include "Game/control/control.h"
#include "Game/Lara/lara_fire.h"

namespace TEN::Scripting
{
	/// Constants for WeaponFlashType.
	// To be used with @{Objects.LaraObject.SpawnGunFlash} function.
	// <br>
	// @enum Objects.WeaponFlashMode
	// @pragma nostrip

	static const auto WEAPON_FLASH_MODE = std::unordered_map<std::string, WeaponFlashMode>
	{
		/// Gunflash is auto determined based on weapon.
		// @mem AUTO
		{ "AUTO", WeaponFlashMode::Auto},

		/// Only Left Gunflash is spawned if weapon suupports it.
		// @mem LEFT
		{ "LEFT", WeaponFlashMode::Left },

		/// Only Right Gunflash is spawned if weapon suupports it..
		// @mem RIGHT
		{ "RIGHT", WeaponFlashMode::Right },
	};
}
