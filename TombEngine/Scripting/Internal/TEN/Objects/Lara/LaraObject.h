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
	LaraWeaponType GetWeaponType() const;
	void SetWeaponType(LaraWeaponType weaponType, bool activate);
	int GetAmmoType() const;
	int GetAmmoCount() const;

	void UndrawWeapon();
	void DiscardTorch();
	bool IsTorchLit() const;

	void Interact(const Moveable& mov, TypeOrNil<int> animNumber,
				  const TypeOrNil<Vec3>& offset, const TypeOrNil<Vec3>& offsetConstraintMin, const TypeOrNil<Vec3>& offsetConstraintMax,
				  const TypeOrNil<Rotation>& rotConstraintMin, const TypeOrNil<Rotation>& rotConstraintMax, TypeOrNil<InputActionID> actionID) const;
	bool TestInteraction(const Moveable& mov,
						 const TypeOrNil<Vec3>& offsetConstraintMin, const TypeOrNil<Vec3>& offsetConstraintMax,
						 const TypeOrNil<Rotation>& rotConstraintMin, const TypeOrNil<Rotation>& rotConstraintMax) const;
	
	using Moveable::Moveable;
};
