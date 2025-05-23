#pragma once

#include "Game/Lara/lara_struct.h"

/// Constants for player weapon ammo types.
// @enum Objects.AmmoType
// @pragma nostrip

/// Table of Objects.AmmoType constants.
// To be used with @{Objects.LaraObject.GetAmmoType} function.
//
// - `NONE`
// - `PISTOLS`
// - `REVOLVER`
// - `UZI`
// - `SHOTGUN_NORMAL`
// - `SHOTGUN_WIDE`
// - `HK`
// - `CROSSBOW_BOLT_NORMAL`
// - `CROSSBOW_BOLT_POISON`
// - `CROSSBOW_BOLT_EXPLOSIVE`
// - `GRENADE_NORMAL`
// - `GRENADE_FRAG`
// - `GRENADE_FLASH`
// - `HARPOON`
// - `ROCKET`
//
// @table Objects.AmmoType

namespace TEN::Scripting
{
	static const auto AMMO_TYPES = std::unordered_map<std::string, PlayerAmmoType>
	{
		{ "PISTOLS", PlayerAmmoType::Pistol },
		{ "REVOLVER", PlayerAmmoType::Revolver },
		{ "UZI", PlayerAmmoType::Uzi },
		{ "SHOTGUN_NORMAL", PlayerAmmoType::ShotgunNormal },
		{ "SHOTGUN_WIDE", PlayerAmmoType::ShotgunWide },
		{ "HK", PlayerAmmoType::HK },
		{ "BOLT_NORMAL", PlayerAmmoType::CrossbowBoltNormal },
		{ "BOLT_POISON", PlayerAmmoType::CrossbowBoltPoison },
		{ "BOLT_EXPLOSIVE", PlayerAmmoType::CrossbowBoltExplosive },
		{ "GRENADE_NORMAL", PlayerAmmoType::GrenadeNormal },
		{ "GRENADE_FRAG", PlayerAmmoType::GrenadeFrag },
		{ "GRENADE_FLASH", PlayerAmmoType::GrenadeFlash },
		{ "HARPOON", PlayerAmmoType::Harpoon },
		{ "ROCKET", PlayerAmmoType::Rocket }
	};
}
