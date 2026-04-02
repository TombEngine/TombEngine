#include "framework.h"
#include "LaraObject.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/effects/item_fx.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Lara/lara_two_guns.h"
#include "Objects/Generic/Object/burning_torch.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/TEN/Input/ActionIDs.h"
#include "Scripting/Internal/TEN/Objects/Lara/AmmoTypes.h"
#include "Scripting/Internal/TEN/Objects/Lara/WeaponModes.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/Input/Input.h"
#include "Specific/Input/InputAction.h"
#include "Specific/level.h"

using namespace TEN::Gui;
using namespace TEN::Hud;
using namespace TEN::Input;

/// Class for player-only functions.
// Do not try to create an object of this type. Use the built-in *Lara* variable instead.
// LaraObject inherits all the functions of @{Objects.Moveable|Moveable}.
//
// @tenclass Objects.LaraObject
// @inherits Objects.Moveable
// @pragma nostrip

constexpr auto LUA_CLASS_NAME{ ScriptReserved_LaraObject };
using namespace TEN::Entities::Generic;

/// Set the player's poison value.
// @function LaraObject:SetPoison
// @tparam[opt=0] int poison New poison value. _Default: 0, Maximum: 128._
// @usage
// Lara:SetPoison(10)
void LaraObject::SetPoison(sol::optional<int> potency)
{
	auto* lara = GetLaraInfo(_moveable);

	if (potency.has_value())
	{
		lara->Status.Poison = std::clamp(potency.value(), 0, (int)LARA_POISON_MAX);
	}
	else
	{
		lara->Status.Poison = 0;
	}
}

/// Get the player's poison value.
// @function LaraObject:GetPoison
// @treturn int Poison value.
// @usage
// local poisonPotency = Lara:GetPoison()
int LaraObject::GetPoison() const
{
	auto* lara = GetLaraInfo(_moveable);
	return lara->Status.Poison;
}

/// Set the player's air value.
// @function LaraObject:SetAir
// @tparam[opt=1800] int air New air value. _Maximum: 1800._
// @usage
// Lara:SetAir(100)
void LaraObject::SetAir(sol::optional<int> air)
{
	auto* lara = GetLaraInfo(_moveable);

	if (air.has_value())
		lara->Status.Air = std::clamp(air.value(), 0, (int)LARA_AIR_MAX);
	else
		lara->Status.Air = LARA_AIR_MAX;
}

/// Get the player's air value.
// @function LaraObject:GetAir
// @treturn int Air value.
// @usage
// local currentAir = Lara:GetAir()
int LaraObject::GetAir() const
{
	auto* lara = GetLaraInfo(_moveable);
	return lara->Status.Air;
}

/// Set the player's wetness value, causing drips.
// @function LaraObject:SetWet
// @tparam[opt=64] int wetness New wetness value. _Maximum: 255._
// @usage
// Lara:SetWet(100)
void LaraObject::SetWet(sol::optional<int> wetness)
{
	auto* lara = GetLaraInfo(_moveable);

	float value = wetness.has_value() ? (float)wetness.value() : PLAYER_DRIP_NODE_MAX;
	for (float& i : lara->Effect.DripNodes)
		i = value;
}

/// Get the player's wetness value.
// @function LaraObject:GetWet
// @treturn int Wetness value.
// @usage
// local dripAmount = Lara:GetWet()
int LaraObject::GetWet() const
{
	auto* lara = GetLaraInfo(_moveable);
	return lara->Effect.DripNodes[0];
}

/// Set the player's stamina value.
// @function LaraObject:SetStamina
// @tparam[opt=120] int New stamina value. _Maximum: 120._
// @usage
// Lara:SetStamina(120)
void LaraObject::SetStamina(sol::optional<int> value)
{
	auto* lara = GetLaraInfo(_moveable);

	if (value.has_value())
	{
		lara->Status.Stamina = std::clamp(value.value(), 0, (int)LARA_STAMINA_MAX);
	}
	else
	{
		lara->Status.Stamina = LARA_STAMINA_MAX;
	}
}

/// Get the player's stamina value.
// @function LaraObject:GetStamina
// @treturn int Stamina value.
// @usage
// local sprintEnergy = Lara:GetStamina()
int LaraObject::GetStamina() const
{
	auto* lara = GetLaraInfo(_moveable);
	return lara->Status.Stamina;
}

/// Get the player's airborne status (set when jumping and falling).
// @function LaraObject:GetAirborne
// @treturn bool True if airborne, otherwise false.
bool LaraObject::GetAirborne() const
{
	return _moveable->Animation.IsAirborne;
}

/// Set the player's airborne status.
// @function LaraObject:SetAirborne
// @tparam bool airborne New airborne status.
void LaraObject::SetAirborne(bool newAirborne)
{
	_moveable->Animation.IsAirborne = newAirborne;
}

/// Undraw a weapon if it is drawn and throw away a flare if currently holding one.
// @function LaraObject:UndrawWeapon
// @usage
// Lara:UndrawWeapon()
void LaraObject::UndrawWeapon()
{
	auto* lara = GetLaraInfo(_moveable);

	if (lara->Control.HandStatus != HandStatus::Free ||
		lara->Control.Weapon.GunType == LaraWeaponType::Flare)
	{
		lara->Control.HandStatus = HandStatus::WeaponUndraw;
	}
}

/// Discard a held torch.
// @function LaraObject:DiscardTorch
// @usage
// Lara:DiscardTorch()
void LaraObject::DiscardTorch()
{
	auto& player = GetLaraInfo(*_moveable);

	if (player.Control.Weapon.GunType == LaraWeaponType::Torch)
		player.Torch.State = TorchState::Dropping;
}

/// Get the player's hand status.
// @function LaraObject:GetHandStatus
// @usage
// local handStatus = Lara:GetHandStatus()
// @treturn Objects.HandStatus Hand status.
HandStatus LaraObject::GetHandStatus() const
{
	auto* lara = GetLaraInfo(_moveable);
	return  HandStatus{ lara->Control.HandStatus };
}

/// Set the player's hand status.
// @function LaraObject:SetHandStatus
// @tparam Objects.HandStatus status Status type to set.
void LaraObject::SetHandStatus(HandStatus status)
{
	auto* lara = GetLaraInfo(_moveable);
	lara->Control.HandStatus = status;
}

/// Get the player's weapon type.
// @function LaraObject:GetWeaponType
// @usage
// local weaponType = Lara:GetWeaponType()
// @treturn Objects.WeaponType Current weapon type.
LaraWeaponType LaraObject::GetWeaponType() const
{
	auto* lara = GetLaraInfo(_moveable);
	return LaraWeaponType{ lara->Control.Weapon.GunType };
}

/// Set the player's weapon type.
// @function LaraObject:SetWeaponType
// @usage
// Lara:SetWeaponType(WeaponType.PISTOLS, false)
// @tparam Objects.WeaponType weaponType New weapon type to set.
// @tparam[opt=false] bool activate If `true`, also draw the weapons or set torch lit. If `false`, keep weapons holstered or leave torch unlit.
void LaraObject::SetWeaponType(LaraWeaponType weaponType, sol::optional<bool> activate)
{
	auto* lara = GetLaraInfo(_moveable);

	switch (weaponType)
	{
	case LaraWeaponType::Flare:
		lara->Control.Weapon.RequestGunType = LaraWeaponType::Flare;
		break;

	case LaraWeaponType::Torch:
		GetFlameTorch();
		lara->Torch.IsLit = activate.value_or(false);
		break;

	default:
		if (!lara->Weapons[(int)weaponType].Present)
		{
			TENLog("SetWeaponType: no such weapon is present in the inventory.", LogLevel::Warning, LogConfig::All);
			break;
		}

		bool weaponInHands = !activate.value_or(false) && (lara->Control.HandStatus == HandStatus::WeaponReady || lara->Control.HandStatus == HandStatus::WeaponDraw);

		if (activate.value_or(false) || weaponInHands)
		{
			lara->Control.Weapon.RequestGunType = weaponType;

			if (weaponInHands)
				TENLog("SetWeaponType: can't switch weapon without activation because another weapon is drawn.", LogLevel::Warning, LogConfig::All);

			if (lara->Control.HandStatus == HandStatus::Free &&
				lara->Control.Weapon.GunType == lara->Control.Weapon.RequestGunType)
			{
				lara->Control.HandStatus = HandStatus::WeaponDraw;
			}
		}
		else
		{
			lara->Control.Weapon.LastGunType = weaponType;

			if (weaponType < LaraWeaponType::Shotgun)
			{
				UndrawPistolMesh(*_moveable, lara->Control.Weapon.LastGunType, true);
				UndrawPistolMesh(*_moveable, lara->Control.Weapon.LastGunType, false);
			}
			else
			{
				UndrawShotgunMeshes(*_moveable, lara->Control.Weapon.LastGunType);
			}
		}
		break;
	}
}

/// Get player lasersight status.
// @function LaraObject:GetLaserSight
// @tparam Objects.WeaponType weaponType Player weapon type. Only works for HK, Revolver and Crossbow.
// @treturn bool Returns true if lasersight is connected to specified weapon.
// @usage
// local test = Lara:GetLaserSight(WeaponType.CROSSBOW)
bool LaraObject::GetLaserSight(LaraWeaponType weaponType) const
{
	auto* lara = GetLaraInfo(_moveable);

	switch (weaponType)
	{
	case LaraWeaponType::Revolver:
	case LaraWeaponType::Crossbow:
	case LaraWeaponType::HK:
		return lara->Weapons[static_cast<int>(weaponType)].HasLasersight;

	default:
		return false;
	}
}

/// Set player lasersight status.
// @function LaraObject:SetLaserSight
// @tparam Objects.WeaponType weaponType Player weapon type to attach lasersight to. Only works for HK, Revolver and Crossbow.
// @tparam[opt=false] bool activate If `true`, also draw the weapons. If `false`, keep weapons holstered.
// @usage
// local test = Lara:SetLaserSight(WeaponType.CROSSBOW)
void LaraObject::SetLaserSight(LaraWeaponType weaponType, TypeOrNil<bool> activate)
{
	auto* lara = GetLaraInfo(_moveable);

	auto convertedActivate = ValueOr<bool>(activate, false);

	//Check for inventory to have lasersight
	if (!lara->Inventory.HasLasersight && convertedActivate)
		return;

	switch (weaponType)
	{
	case LaraWeaponType::Revolver:
	case LaraWeaponType::Crossbow:
	case LaraWeaponType::HK:

		// Check if laser sight is already attached to any other weapon
		for (LaraWeaponType type : { LaraWeaponType::Revolver, LaraWeaponType::Crossbow, LaraWeaponType::HK })
		{
			if (type != weaponType && lara->Weapons[static_cast<int>(type)].HasLasersight)
			{
				// Laser sight is already in use, do nothing
				return;
			}
		}

		// Attach laser sight
		lara->Weapons[static_cast<int>(weaponType)].HasLasersight = convertedActivate;

		// Activate weapon if required
		if (convertedActivate == false)
			lara->Control.Weapon.LastGunType = weaponType;
		else
			lara->Control.Weapon.RequestGunType = weaponType;
		break;

	default:
		break;
	}
}

/// Get player weapon ammo type.
// @function LaraObject:GetAmmoType
// @tparam[opt] Objects.WeaponType weaponType Weapon to retrieve ammo type for. If omitted, the ammo type of the currently equipped weapon is returned.
// @treturn Objects.AmmoType Player weapon ammo type.
// @usage
// local CurrentAmmoType = Lara:GetAmmoType()
int LaraObject::GetAmmoType(TypeOrNil<LaraWeaponType> weaponType) const
{
	const auto& player = GetLaraInfo(*_moveable);

	auto weapon = ValueOr<LaraWeaponType>(weaponType, player.Control.Weapon.GunType);
	auto ammoType = std::optional<PlayerAmmoType>(std::nullopt);

	switch (weapon)
	{
		case::LaraWeaponType::Pistol:
			ammoType = PlayerAmmoType::Pistol;
			break;

		case::LaraWeaponType::Revolver:
			ammoType = PlayerAmmoType::Revolver;
			break;

		case::LaraWeaponType::Uzi:
			ammoType = PlayerAmmoType::Uzi;
			break;

		case::LaraWeaponType::Shotgun:
			if (player.Weapons[(int)LaraWeaponType::Shotgun].SelectedAmmo == WeaponAmmoType::Ammo1)
			{
				ammoType = PlayerAmmoType::ShotgunNormal;
			}
			else
			{
				ammoType = PlayerAmmoType::ShotgunWide;
			}

			break;

		case::LaraWeaponType::HK:
			ammoType = PlayerAmmoType::HK;
			break;

		case::LaraWeaponType::Crossbow:
			if (player.Weapons[(int)LaraWeaponType::Crossbow].SelectedAmmo == WeaponAmmoType::Ammo1)
			{
				ammoType = PlayerAmmoType::CrossbowBoltNormal;
			}
			else if (player.Weapons[(int)LaraWeaponType::Crossbow].SelectedAmmo == WeaponAmmoType::Ammo2)
			{
				ammoType = PlayerAmmoType::CrossbowBoltPoison;
			}
			else
			{
				ammoType = PlayerAmmoType::CrossbowBoltExplosive;
			}

			break;

		case::LaraWeaponType::GrenadeLauncher:
			if (player.Weapons[(int)LaraWeaponType::GrenadeLauncher].SelectedAmmo == WeaponAmmoType::Ammo1)
			{
				ammoType = PlayerAmmoType::GrenadeNormal;
			}
			else if (player.Weapons[(int)LaraWeaponType::GrenadeLauncher].SelectedAmmo == WeaponAmmoType::Ammo2)
			{
				ammoType = PlayerAmmoType::GrenadeFrag;
			}
			else
			{
				ammoType = PlayerAmmoType::GrenadeFlash;
			}

			break;

		case::LaraWeaponType::HarpoonGun:
			ammoType = PlayerAmmoType::Harpoon;
			break;

		case::LaraWeaponType::RocketLauncher:
			ammoType = PlayerAmmoType::Rocket;
			break;

		default:
			break;
	}

	if (!ammoType.has_value())
	{
		TENLog("GetAmmoType error; no ammo type.", LogLevel::Warning, LogConfig::All);
		ammoType = PlayerAmmoType::None;
	}

	return (int)*ammoType;
}

/// Set player weapon ammo type.
// @function LaraObject:SetAmmoType
// @tparam Objects.AmmoType Player weapon ammo type.
// @usage
// Lara:SetAmmoType(TEN.Objects.AmmoType.CROSSBOW_BOLT_NORMAL)
void LaraObject::SetAmmoType(PlayerAmmoType ammoType)
{
	auto& player = GetLaraInfo(*_moveable);

	switch (ammoType)
	{
	case PlayerAmmoType::ShotgunNormal:
		player.Weapons[(int)LaraWeaponType::Shotgun].SelectedAmmo = WeaponAmmoType::Ammo1;
		break;

	case PlayerAmmoType::ShotgunWide:
		player.Weapons[(int)LaraWeaponType::Shotgun].SelectedAmmo = WeaponAmmoType::Ammo2;
		break;

	case PlayerAmmoType::CrossbowBoltNormal:
		player.Weapons[(int)LaraWeaponType::Crossbow].SelectedAmmo = WeaponAmmoType::Ammo1;
		break;

	case PlayerAmmoType::CrossbowBoltPoison:
		player.Weapons[(int)LaraWeaponType::Crossbow].SelectedAmmo = WeaponAmmoType::Ammo2;
		break;

	case PlayerAmmoType::CrossbowBoltExplosive:
		player.Weapons[(int)LaraWeaponType::Crossbow].SelectedAmmo = WeaponAmmoType::Ammo3;
		break;

	case PlayerAmmoType::GrenadeNormal:
		player.Weapons[(int)LaraWeaponType::GrenadeLauncher].SelectedAmmo = WeaponAmmoType::Ammo1;
		break;

	case PlayerAmmoType::GrenadeFrag:
		player.Weapons[(int)LaraWeaponType::GrenadeLauncher].SelectedAmmo = WeaponAmmoType::Ammo2;
		break;

	case PlayerAmmoType::GrenadeFlash:
		player.Weapons[(int)LaraWeaponType::GrenadeLauncher].SelectedAmmo = WeaponAmmoType::Ammo3;
		break;

	default:
		break;
	}
}

/// Get current weapon's ammo count.
// @function LaraObject:GetAmmoCount
// @treturn int Current ammo count (-1 if infinite).
// @usage
// local equippedWeaponAmmoLeft = Lara:GetAmmoCount()
int LaraObject::GetAmmoCount() const
{
	auto* lara = GetLaraInfo(_moveable);
	auto& ammo = GetAmmo(Lara, Lara.Control.Weapon.GunType);
	return (ammo.HasInfinite()) ? -1 : (int)ammo.GetCount();
}

/// Get player weapon mode type.
// @function LaraObject:GetWeaponMode
// @tparam[opt] Objects.WeaponType weaponType Weapon to retrieve weapon mode for. If omitted, the mode of the currently equipped weapon is returned. Only works for HK weapon currently.
// @treturn Objects.WeaponMode Player weapon mode type.
int LaraObject::GetWeaponMode(TypeOrNil<LaraWeaponType> weaponType) const
{
	const auto& player = GetLaraInfo(*_moveable);

	auto weapon = ValueOr<LaraWeaponType>(weaponType, player.Control.Weapon.GunType);
	auto weaponMode = std::optional<PlayerWeaponMode>(std::nullopt);

	switch (weapon)
	{
	case::LaraWeaponType::HK:
		if (player.Weapons[(int)LaraWeaponType::HK].WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_1)
		{
			weaponMode = PlayerWeaponMode::Rapid;
			break;
		}
		else if (player.Weapons[(int)LaraWeaponType::HK].WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_2)
		{
			weaponMode = PlayerWeaponMode::Burst;
			break;
		}
		else if (player.Weapons[(int)LaraWeaponType::HK].WeaponMode == LaraWeaponTypeCarried::WTYPE_AMMO_3)
		{
			weaponMode = PlayerWeaponMode::Sniper;
			break;
		}
		break;

	default:
		break;
	}

	if (!weaponMode.has_value())
	{
		TENLog("GetWeaponMode: no weapon mode is available for specified weapon.", LogLevel::Warning, LogConfig::All);
		weaponMode = PlayerWeaponMode::None;
	}

	return static_cast<int>(weaponMode.value());
}

/// Set player weapon mode type.
// @function LaraObject:SetWeaponMode
// @tparam Objects.WeaponType weaponType Weapon to set weapon mode for. Only works for HK weapon currently.
// @tparam Objects.WeaponMode weaponMode Player weapon mode type.
void LaraObject::SetWeaponMode(LaraWeaponType weaponType, PlayerWeaponMode weaponMode)
{
	auto& player = GetLaraInfo(*_moveable);

	switch (weaponType)
	{
	case::LaraWeaponType::HK:
		switch (weaponMode)
		{
		case PlayerWeaponMode::Rapid:
			player.Weapons[(int)LaraWeaponType::HK].WeaponMode = LaraWeaponTypeCarried::WTYPE_AMMO_1;
			break;

		case PlayerWeaponMode::Burst:
			player.Weapons[(int)LaraWeaponType::HK].WeaponMode = LaraWeaponTypeCarried::WTYPE_AMMO_2;
			break;

		case PlayerWeaponMode::Sniper:
			player.Weapons[(int)LaraWeaponType::HK].WeaponMode = LaraWeaponTypeCarried::WTYPE_AMMO_3;
			break;

		default:
			TENLog("SetWeaponMode: unsupported weapon mode for HK weapon type.", LogLevel::Warning, LogConfig::All);
			break;
		}
		break;

	default:
		TENLog("SetWeaponMode: no weapon mode supported for weapon type.", LogLevel::Warning, LogConfig::All);
		break;
	}
}

/// Get current vehicle, if it exists.
// @function LaraObject:GetVehicle
// @treturn Objects.Moveable Current vehicle (nil if no vehicle present).
// @usage
// local vehicle = Lara:GetVehicle()
std::unique_ptr<Moveable> LaraObject::GetVehicle() const
{
	auto* lara = GetLaraInfo(_moveable);

	if (lara->Context.Vehicle == NO_VALUE)
		return nullptr;

	return std::make_unique<Moveable>(lara->Context.Vehicle);
}

/// Get the player's current targeted moveable (if it exists).
// @function LaraObject:GetTarget
// @treturn Objects.Moveable Target moveable (nil if the player is not currently targeting a moveable).
// @usage
// local target = Lara:GetTarget()
std::unique_ptr<Moveable> LaraObject::GetTarget() const
{
	const auto& player = GetLaraInfo(*_moveable);

	if (player.TargetEntity == nullptr)
		return nullptr;

	return std::make_unique<Moveable>(player.TargetEntity->Index);
}

/// Get the player's current interacted moveable (if it exists).
// @function LaraObject:GetInteractedMoveable
// @treturn Objects.Moveable Interacted moveable (nil if the player is not interacting with a moveable).
// @usage
// local interactedMoveable = Lara:GetInteractedMoveable()
std::unique_ptr<Moveable> LaraObject::GetPlayerInteractedMoveable() const
{
	const auto& player = GetLaraInfo(*_moveable);

	if (player.Context.InteractedItem == NO_VALUE)
		return nullptr;

	return std::make_unique<Moveable>(player.Context.InteractedItem);
}

/// Check if a held torch is lit.
// @function LaraObject:IsTorchLit
// @treturn bool True if lit, otherwise false (also false if there is no torch in hand).
// @usage
// local isTorchLit = Lara:IsTorchLit()
bool LaraObject::IsTorchLit() const
{
	const auto& player = GetLaraInfo(*_moveable);
	return player.Torch.IsLit;
}

/// Get player water status.
// @function LaraObject:GetWaterStatus
// @treturn Objects.WaterStatus Current water status of player.
// @usage
// local waterStatus = Lara:GetWaterStatus()
WaterStatus LaraObject::GetWaterStatus() const
{
	auto* lara = GetLaraInfo(_moveable);
	return  WaterStatus{ lara->Control.WaterStatus };
}

/// Get player water skin amount.
// @function LaraObject:GetWaterSkinStatus
// @tparam bool flag use True get the amount for Big Waterskin. False for Small Waterskin.
// @treturn int Current capacity of specified water skin. For Smallwater skin (0 = Waterskin not present; 1 = Waterskin Empty; 2 = Waterskin 1L; 3 = WaterSkin 2L; 4 = Waterskin 3L).
// For Largewater skin (0 = Waterskin not present; 1 = Waterskin Empty; 2 = Waterskin 1L; 3 = WaterSkin 2L; 4 = Waterskin 3L; 5 = Waterskin 4L; 6 = Waterskin 5L)
// @usage
// local bigWaterSkinCapacity = Lara:GetWaterSkinStatus(true)
// local smallWaterSkinCapacity = Lara:GetWaterSkinStatus(false)
int LaraObject::GetWaterSkinStatus(TypeOrNil<bool> flag) const
{
	auto convertedFlag = ValueOr<bool>(flag, false);

	const auto& inventory = GetLaraInfo(*_moveable).Inventory;
	const auto value = convertedFlag ? inventory.BigWaterskin : inventory.SmallWaterskin;
	return value;
}

/// Set player water skin amount.
// @function LaraObject:SetWaterSkinStatus
// @tparam int amount Current capacity of specified water skin. For Smallwater skin (0 = Waterskin not present; 1 = Waterskin Empty; 2 = Waterskin 1L; 3 = WaterSkin 2L; 4 = Waterskin 3L).
// For Largewater skin (0 = Waterskin not present; 1 = Waterskin Empty; 2 = Waterskin 1L; 3 = WaterSkin 2L; 4 = Waterskin 3L; 5 = Waterskin 4L; 6 = Waterskin 5L)
// @tparam bool flag use True get the amount for Big Waterskin. False for Small Waterskin.
// @usage
// local bigWaterSkinCapacity = Lara:SetWaterSkinStatus(2)
// local smallWaterSkinCapacity = Lara:SetWaterSkinStatus(3)
void LaraObject::SetWaterSkinStatus(int amount, TypeOrNil<bool> flag)
{
	auto convertedFlag = ValueOr<bool>(flag, false);
	auto& inventory = GetLaraInfo(*_moveable).Inventory;

	const int maxValue = convertedFlag ? 6 : 4; // small = 4 (1 base + 3 liters), big = 6 (1 base + 5 liters)

	amount = std::clamp(amount, 0, maxValue);

	if (convertedFlag)
		inventory.BigWaterskin = amount;
	else
		inventory.SmallWaterskin = amount;
}

/// Align the player with a moveable object for interaction.
// @function LaraObject:Interact
// @tparam Objects.Moveable mov Moveable object to align the player with.
// @tparam[opt=197 (BUTTON_PUSH)] int animNumber The animation to play after alignment is complete.
// @tparam[opt=Vec3(0&#44; 0&#44; 312)] Vec3 offset Relative position offset from the moveable.
// @tparam[opt=Vec3(-256&#44; -512&#44; 0)] Vec3 minOffsetConstraint Minimum relative offset constraint.
// @tparam[opt=Vec3(256&#44; 0&#44; 512)] Vec3 maxOffsetConstraint Maximum relative offset constraint.
// @tparam[opt=Rotation(-10&#44; -40&#44; -10)] Rotation minRotConstraint Minimum relative rotation constraint.
// @tparam[opt=Rotation(10&#44; 40&#44; 10)] Rotation maxRotConstraint Maximum relative rotation constraint.
// @tparam[opt=Input.ActionID.ACTION] Input.ActionID actionID Input action ID to trigger the alignment.
// @tparam[opt] Objects.ObjID objectID Object ID required in inventory for interaction.
// @tparam[opt] Objects.InteractionType interactionType Interaction icon type to show.
// @usage
// local Lara:Interact(
//     moveable, 197,
//     Vec3(0, 0, 312), Vec3(-256, -512, -256), Vec3(256, 0, 512),
//	   Rotation(-10, -30, -10), Rotation(10, 30, 10), TEN.Input.ActionID.ACTION)
void LaraObject::Interact(const Moveable& mov, TypeOrNil<int> animNumber,
						  const TypeOrNil<Vec3>& offset, const TypeOrNil<Vec3>& offsetConstraintMin, const TypeOrNil<Vec3>& offsetConstraintMax,
						  const TypeOrNil<Rotation>& rotConstraintMin, const TypeOrNil<Rotation>& rotConstraintMax, TypeOrNil<ActionID> actionID,
							TypeOrNil<GAME_OBJECT_ID> objectID, const TypeOrNil<InteractionType> interactionType) const
{
	auto convertedOffset = ValueOr<Vec3>(offset, Vec3(0.0f, 0.0f, BLOCK(0.305f))).ToVector3i();
	auto convertedOffsetConstraintMin = ValueOr<Vec3>(offsetConstraintMin, Vec3(-BLOCK(0.25f), -BLOCK(0.5f), 0.0f));
	auto convertedOffsetConstraintMax = ValueOr<Vec3>(offsetConstraintMax, Vec3(BLOCK(0.25f), 0.0f, BLOCK(0.5f)));
	auto convertedRotConstraintMin = ValueOr<Rotation>(rotConstraintMin, Rotation(-10.0f, -40.0f, -10.0f)).ToEulerAngles();
	auto convertedRotConstraintMax = ValueOr<Rotation>(rotConstraintMax, Rotation(10.0f, 40.0f, 10.0f)).ToEulerAngles();
	int convertedAnimNumber = ValueOr<int>(animNumber, LA_BUTTON_SMALL_PUSH);
	auto convertedActionID = ValueOr<ActionID>(actionID, In::Action);
	auto convertedObjectID = ValueOr<GAME_OBJECT_ID>(objectID, ID_NO_OBJECT);
	auto convertedIcon = ValueOr<InteractionType>(interactionType, InteractionType::Undefined);

	auto interactionBasis = ObjectCollisionBounds
	{
		GameBoundingBox(
			convertedOffsetConstraintMin.x, convertedOffsetConstraintMax.x,
			convertedOffsetConstraintMin.y, convertedOffsetConstraintMax.y,
			convertedOffsetConstraintMin.z, convertedOffsetConstraintMax.z),
		std::pair(
			convertedRotConstraintMin,
			convertedRotConstraintMax)
	};

	auto& player = GetLaraInfo(*_moveable);
	auto& interactedItem = g_Level.Items[mov.GetIndex()];

	bool isUnderwater = (player.Control.WaterStatus == WaterStatus::Underwater);
	bool isPlayerIdle = ((!isUnderwater && _moveable->Animation.ActiveState == LS_IDLE && _moveable->Animation.AnimNumber == LA_STAND_IDLE) ||
						 (isUnderwater && _moveable->Animation.ActiveState == LS_UNDERWATER_IDLE && _moveable->Animation.AnimNumber == LA_UNDERWATER_IDLE));

	if (convertedIcon != InteractionType::Undefined)
		g_Hud.InteractionHighlighter.Test(*LaraItem, interactedItem, InteractionMode::Always, convertedIcon);

	bool movingWithSameItem =
		player.Control.IsMoving &&
		player.Context.InteractedItem == interactedItem.Index;

	bool canIdleInteract =
		player.Control.HandStatus == HandStatus::Free &&
		isPlayerIdle;

	bool wantsInteraction =
		IsHeld(convertedActionID) ||
		(convertedObjectID != ID_NO_OBJECT &&
			g_Gui.GetInventoryItemChosen() != NO_VALUE);

	if (!movingWithSameItem && !canIdleInteract)
		return;

	if (!movingWithSameItem && !wantsInteraction)
		return;

	if (!TestLaraPosition(interactionBasis, &interactedItem, _moveable))
		return;

	if (convertedObjectID != ID_NO_OBJECT && !player.Control.IsMoving)
	{
		if (g_Gui.GetInventoryItemChosen() == NO_VALUE)
		{
			if (g_Gui.IsObjectInInventory(convertedObjectID))
				g_Gui.SetEnterInventory(convertedObjectID);
			else if (IsClicked(convertedActionID))
				SayNo();

			return;
		}

		if (g_Gui.GetInventoryItemChosen() != convertedObjectID)
			return;

		player.Context.InteractedItem = interactedItem.Index;
	}

	if (player.Context.InteractedItem != interactedItem.Index &&
		convertedObjectID != ID_NO_OBJECT)
		return;

	if (MoveLaraPosition(convertedOffset, &interactedItem, _moveable))
	{
		ResetPlayerFlex(_moveable);
		SetAnimation(_moveable, convertedAnimNumber);

		player.Control.IsMoving = false;
		player.Control.HandStatus = HandStatus::Busy;
	}
	else
	{
		player.Context.InteractedItem = interactedItem.Index;
	}

	if (convertedObjectID != ID_NO_OBJECT)
		g_Gui.SetInventoryItemChosen(NO_VALUE);
}

/// Test the player against a moveable object for interaction.
// @function LaraObject:TestInteraction
// @tparam Objects.Moveable mov Moveable object to align the player with.
// @tparam[opt=Vec3(-256&#44; -512&#44; 0)] Vec3 minOffsetConstraint Minimum relative offset constraint.
// @tparam[opt=Vec3(256&#44; 0&#44; 512)] Vec3 maxOffsetConstraint Maximum relative offset constraint.
// @tparam[opt=Rotation(-10&#44; -40&#44; -10)] Rotation minRotConstraint Minimum relative rotation constraint.
// @tparam[opt=Rotation(10&#44; 40&#44; 10)] Rotation maxRotConstraint Maximum relative rotation constraint.
// @treturn bool Returns true if the player is inside the specified bounds.
bool LaraObject::TestInteraction(const Moveable& mov,
								 const TypeOrNil<Vec3>& offsetConstraintMin, const TypeOrNil<Vec3>& offsetConstraintMax,
								 const TypeOrNil<Rotation>& rotConstraintMin, const TypeOrNil<Rotation>& rotConstraintMax) const
{
	auto convertedOffsetConstraintMin = ValueOr<Vec3>(offsetConstraintMin, Vec3(-BLOCK(0.25f), -BLOCK(0.5f), 0));
	auto convertedOffsetConstraintMax = ValueOr<Vec3>(offsetConstraintMax, Vec3(BLOCK(0.25f), 0, BLOCK(0.5f)));
	auto convertedRotConstraintMin = ValueOr<Rotation>(rotConstraintMin, Rotation(-10.0f, -40.0f, -10.0f)).ToEulerAngles();
	auto convertedRotConstraintMax = ValueOr<Rotation>(rotConstraintMax, Rotation(10.0f, 40.0f, 10.0f)).ToEulerAngles();

	auto interactionBasis = ObjectCollisionBounds
	{
		GameBoundingBox(
			convertedOffsetConstraintMin.x, convertedOffsetConstraintMax.x,
			convertedOffsetConstraintMin.y, convertedOffsetConstraintMax.y,
			convertedOffsetConstraintMin.z, convertedOffsetConstraintMax.z),
		std::pair(
			convertedRotConstraintMin,
			convertedRotConstraintMax)
	};

	auto& item = g_Level.Items[mov.GetIndex()];
	return (TestLaraPosition(interactionBasis, &item, _moveable));
}

void LaraObject::Register(sol::table& parent)
{
	parent.new_usertype<LaraObject>(
		LUA_CLASS_NAME,

		ScriptReserved_SetPoison, &LaraObject::SetPoison,
		ScriptReserved_GetPoison, &LaraObject::GetPoison,
		ScriptReserved_SetAir, &LaraObject::SetAir,
		ScriptReserved_GetAir, &LaraObject::GetAir,
		ScriptReserved_SetWet, &LaraObject::SetWet,
		ScriptReserved_GetWet, &LaraObject::GetWet,
		ScriptReserved_SetStamina, &LaraObject::SetStamina,
		ScriptReserved_GetStamina, &LaraObject::GetStamina,
		ScriptReserved_GetAirborne, &LaraObject::GetAirborne,
		ScriptReserved_SetAirborne, &LaraObject::SetAirborne,
		ScriptReserved_UndrawWeapon, &LaraObject::UndrawWeapon,
		ScriptReserved_PlayerDiscardTorch, &LaraObject::DiscardTorch,
		ScriptReserved_GetHandStatus, &LaraObject::GetHandStatus,
		ScriptReserved_SetHandStatus, & LaraObject::SetHandStatus,
		ScriptReserved_GetWeaponType, &LaraObject::GetWeaponType,
		ScriptReserved_SetWeaponType, &LaraObject::SetWeaponType,
		ScriptReserved_GetLaserSight, & LaraObject::GetLaserSight,
		ScriptReserved_SetLaserSight, & LaraObject::SetLaserSight,
		ScriptReserved_GetAmmoType, &LaraObject::GetAmmoType,
		ScriptReserved_SetAmmoType, & LaraObject::SetAmmoType,
		ScriptReserved_GetAmmoCount, &LaraObject::GetAmmoCount,
		ScriptReserved_GetWeaponMode, & LaraObject::GetWeaponMode,
		ScriptReserved_SetWeaponMode, & LaraObject::SetWeaponMode,
		ScriptReserved_GetVehicle, &LaraObject::GetVehicle,
		ScriptReserved_GetTarget, &LaraObject::GetTarget,
		ScriptReserved_GetPlayerInteractedMoveable, &LaraObject::GetPlayerInteractedMoveable,
		ScriptReserved_PlayerIsTorchLit, &LaraObject::IsTorchLit,
		ScriptReserved_GetWaterStatus, & LaraObject::GetWaterStatus,
		ScriptReserved_GetWaterSkinStatus, & LaraObject::GetWaterSkinStatus,
		ScriptReserved_SetWaterSkinStatus, & LaraObject::SetWaterSkinStatus,
		ScriptReserved_PlayerInteract, &LaraObject::Interact,
		ScriptReserved_PlayerTestInteraction, &LaraObject::TestInteraction,

		// COMPATIBILITY
		"TorchIsLit", &LaraObject::IsTorchLit,
		"ThrowAwayTorch", &LaraObject::DiscardTorch,

		sol::base_classes, sol::bases<Moveable>());
}
