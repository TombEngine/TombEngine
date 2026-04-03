#pragma once

#include "Game/Lara/lara_struct.h"
#include "Scripting/Internal/TEN/Input/ActionIDs.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"

// TODO: Organise.
class LaraObject : public Moveable
{
public:
	static void Register(sol::table& parent);

	void SetPoison(sol::optional<int> potency);
	int GetPoison() const;
	void SetAir(sol::optional<int> air);
	int GetAir() const;
	void SetStamina(sol::optional<int> value);
	int GetStamina() const;
	void SetWet(sol::optional<int> wetness);
	int GetWet() const;
	bool GetAirborne() const;
	void SetAirborne(bool newAirborne);

	std::unique_ptr<Moveable> GetVehicle() const;
	std::unique_ptr<Moveable> GetTarget() const;
	std::unique_ptr<Moveable> GetPlayerInteractedMoveable() const;
	HandStatus GetHandStatus() const;
	void SetHandStatus(HandStatus status);
	LaraWeaponType GetWeaponType() const;
	void SetWeaponType(LaraWeaponType weaponType, sol::optional<bool> activate);
	bool GetLaserSight(LaraWeaponType weaponType) const;
	void SetLaserSight(LaraWeaponType weaponType, TypeOrNil<bool> activate);
	int GetAmmoType(TypeOrNil<LaraWeaponType> weaponType) const;
	void SetAmmoType(PlayerAmmoType ammoType);
	int GetAmmoCount() const;
	int GetWeaponMode(TypeOrNil<LaraWeaponType> weaponType) const;
	void SetWeaponMode(LaraWeaponType weaponType, PlayerWeaponMode weaponMode);

	void UndrawWeapon();
	void DiscardTorch();
	bool IsTorchLit() const;

	WaterStatus GetWaterStatus() const;

	int GetWaterSkinStatus(TypeOrNil<bool> flag) const;
	void SetWaterSkinStatus(int amount, TypeOrNil<bool> flag);

	void Interact(const Moveable& mov, TypeOrNil<int> animNumber,
				  const TypeOrNil<Vec3>& offset, const TypeOrNil<Vec3>& offsetConstraintMin, const TypeOrNil<Vec3>& offsetConstraintMax,
				  const TypeOrNil<Rotation>& rotConstraintMin, const TypeOrNil<Rotation>& rotConstraintMax, TypeOrNil<ActionID> actionID,
				TypeOrNil<GAME_OBJECT_ID> objectID, const TypeOrNil<InteractionType> interactionType) const;
	bool TestInteraction(const Moveable& mov,
						 const TypeOrNil<Vec3>& offsetConstraintMin, const TypeOrNil<Vec3>& offsetConstraintMax,
						 const TypeOrNil<Rotation>& rotConstraintMin, const TypeOrNil<Rotation>& rotConstraintMax) const;
	
	using Moveable::Moveable;
};
