#include "framework.h"
#include "LaraObject.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/effects/item_fx.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Objects/Generic/Object/burning_torch.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/TEN/Input/ActionIDs.h"
#include "Scripting/Internal/TEN/Objects/Lara/AmmoTypes.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/Input/Input.h"
#include "Specific/Input/InputAction.h"
#include "Specific/level.h"

using namespace TEN::Input;

/// Class for player-only functions.
// Do not try to create an object of this type. Use the built-in *Lara* variable instead.
// LaraObject inherits all the functions of @{Objects.Moveable|Moveable}.
//
// @tenclass Objects.LaraObject
// @pragma nostrip

constexpr auto LUA_CLASS_NAME{ ScriptReserved_LaraObject };
using namespace TEN::Entities::Generic;

/// Set the player's poison value.
// @function LaraObject:SetPoison
// @tparam[opt] int poison New poison value. __default: 0, max: 128__
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
// @tparam int air New air value. __max: 1800__ 
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
// @tparam int wetness New wetness value. __max: 255__
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
// @tparam int New stamina value. __max: 120__ 
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
// @function Moveable:GetAirborne
// @treturn bool True if airborne, otherwise false.
bool LaraObject::GetAirborne() const
{
	return _moveable->Animation.IsAirborne;
}

/// Set the player's airborne status.
// @function Moveable:SetAirborne
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
// @tparam bool activate If `true`, also draw the weapons or set torch lit. If `false`, keep weapons holstered or leave torch unlit.
void LaraObject::SetWeaponType(LaraWeaponType weaponType, bool activate)
{
	auto* lara = GetLaraInfo(_moveable);

	switch (weaponType)
	{
	case LaraWeaponType::Flare:
		lara->Control.Weapon.RequestGunType = LaraWeaponType::Flare;
		break;

	case LaraWeaponType::Torch:
		GetFlameTorch();
		lara->Torch.IsLit = activate;
		break;

	default:
		if (activate == false)
			lara->Control.Weapon.LastGunType = weaponType;
		else
			lara->Control.Weapon.RequestGunType = weaponType;
		break;
	}
}


/// Get player weapon ammo type.
// @function LaraObject:GetAmmoType
// @treturn Objects.AmmoType Player weapon ammo type.
// @usage
// local CurrentAmmoType = Lara:GetAmmoType()
int LaraObject::GetAmmoType() const
{
	const auto& player = GetLaraInfo(*_moveable);

	auto ammoType = std::optional<PlayerAmmoType>(std::nullopt);
	switch (player.Control.Weapon.GunType)
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
		TENLog("GetAmmoType() error; no ammo type.", LogLevel::Warning, LogConfig::All);
		ammoType = PlayerAmmoType::None;
	}

	return (int)*ammoType;
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

/// Get current vehicle, if it exists.
// @function LaraObject:GetVehicle
// @treturn Objects.Moveable current vehicle (nil if no vehicle present).
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
// @treturn bool True if lit, otherwise false.
// @usage
// local isTorchLit = Lara:IsTorchLit()
bool LaraObject::IsTorchLit() const
{
	const auto& player = GetLaraInfo(*_moveable);
	return player.Torch.IsLit;
}

/// Align the player with a moveable object for interaction.
// @function LaraObject:Interact
// @tparam Moveable mov Moveable object to align the player with.
// @tparam[opt] int animNumber The animation to play after alignment is complete. __default: BUTTON_PUSH__
// @tparam[opt] Vec3 offset Relative position offset from the moveable. __default: Vec3(0, 0, 312)__
// @tparam[opt] Vec3 minOffsetConstraint Minimum relative offset constraint. __default: Vec3(-256, -512, 0)__
// @tparam[opt] Vec3 maxOffsetConstraint Maximum relative offset constraint. __default: Vec3(256, 0, 512)__
// @tparam[opt] Rotation minRotConstraint Minimum relative rotation constraint. __default: Rotation(-10, -40, -10)__
// @tparam[opt] Rotation maxRotConstraint Maximum relative rotation constraint. __default: Rotation(10, 40, 10)__
// @tparam[opt] Input.ActionID actionID Input action ID to trigger the alignment. __default: Input.ActionID.ACTION__
// @usage
// local Lara:Interact(
//     moveable, 197,
//     Vec3(0, 0, 312), Vec3(-256, -512, -256), Vec3(256, 0, 512),
//	   Rotation(-10, -30, -10), Rotation(10, 30, 10), TEN.Input.ActionID.ACTION)
void LaraObject::Interact(const Moveable& mov, TypeOrNil<int> animNumber,
						  const TypeOrNil<Vec3>& offset, const TypeOrNil<Vec3>& offsetConstraintMin, const TypeOrNil<Vec3>& offsetConstraintMax,
						  const TypeOrNil<Rotation>& rotConstraintMin, const TypeOrNil<Rotation>& rotConstraintMax, TypeOrNil<InputActionID> actionID) const
{
	auto convertedOffset = ValueOr<Vec3>(offset, Vec3(0.0f, 0.0f, BLOCK(0.305f))).ToVector3i();
	auto convertedOffsetConstraintMin = ValueOr<Vec3>(offsetConstraintMin, Vec3(-BLOCK(0.25f), -BLOCK(0.5f), 0.0f));
	auto convertedOffsetConstraintMax = ValueOr<Vec3>(offsetConstraintMax, Vec3(BLOCK(0.25f), 0.0f, BLOCK(0.5f)));
	auto convertedRotConstraintMin = ValueOr<Rotation>(rotConstraintMin, Rotation(-10.0f, -40.0f, -10.0f)).ToEulerAngles();
	auto convertedRotConstraintMax = ValueOr<Rotation>(rotConstraintMax, Rotation(10.0f, 40.0f, 10.0f)).ToEulerAngles();
	int convertedAnimNumber = ValueOr<int>(animNumber, LA_BUTTON_SMALL_PUSH);
	auto convertedActionID = ValueOr<InputActionID>(actionID, In::Action);

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

	if ((player.Control.IsMoving && player.Context.InteractedItem == interactedItem.Index) ||
		(IsHeld(convertedActionID) && player.Control.HandStatus == HandStatus::Free && isPlayerIdle))
	{
		if (TestLaraPosition(interactionBasis, &interactedItem, _moveable))
		{
			if (MoveLaraPosition(convertedOffset, &interactedItem, _moveable))
			{
				ResetPlayerFlex(_moveable);
				SetAnimation(_moveable, convertedAnimNumber);

				_moveable->Animation.FrameNumber = GetAnimData(_moveable).frameBase;
				player.Control.IsMoving = false;
				player.Control.HandStatus = HandStatus::Busy;
			}
			else
			{
				player.Context.InteractedItem = interactedItem.Index;
			}
		}
	}
}

/// Test the player against a moveable object for interaction.
// @function LaraObject:TestInteraction
// @tparam Moveable mov Moveable object to align the player with.
// @tparam[opt] Vec3 minOffsetConstraint Minimum relative offset constraint. __default: Vec3(-256, -512, 0)__
// @tparam[opt] Vec3 maxOffsetConstraint Maximum relative offset constraint. __default: Vec3(256, 0, 512)__
// @tparam[opt] Rotation minRotConstraint Minimum relative rotation constraint. __default: Rotation(-10, -40, -10)__
// @tparam[opt] Rotation maxRotConstraint Maximum relative rotation constraint. __default: Rotation(10, 40, 10)__
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
		ScriptReserved_GetWeaponType, &LaraObject::GetWeaponType,
		ScriptReserved_SetWeaponType, &LaraObject::SetWeaponType,
		ScriptReserved_GetAmmoType, &LaraObject::GetAmmoType,
		ScriptReserved_GetAmmoCount, &LaraObject::GetAmmoCount,
		ScriptReserved_GetVehicle, &LaraObject::GetVehicle,
		ScriptReserved_GetTarget, &LaraObject::GetTarget,
		ScriptReserved_GetPlayerInteractedMoveable, &LaraObject::GetPlayerInteractedMoveable,
		ScriptReserved_PlayerIsTorchLit, &LaraObject::IsTorchLit,

		ScriptReserved_PlayerInteract, &LaraObject::Interact,
		ScriptReserved_PlayerTestInteraction, &LaraObject::TestInteraction,

		// COMPATIBILITY
		"TorchIsLit", &LaraObject::IsTorchLit,
		"ThrowAwayTorch", &LaraObject::DiscardTorch,

		sol::base_classes, sol::bases<Moveable>());
}
