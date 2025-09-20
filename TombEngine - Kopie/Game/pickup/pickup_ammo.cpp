#include "framework.h"
#include "Game/pickup/pickup_ammo.h"

#include "Game/Lara/lara_struct.h"
#include "Game/pickup/pickuputil.h"
#include "Objects/objectslist.h"

struct AmmoPickupInfo
{
	GAME_OBJECT_ID ObjectID;
	LaraWeaponType LaraWeaponType;
	WeaponAmmoType AmmoType;
	int Amount;
};

static std::array<AmmoPickupInfo, 14> Ammo
{
	{
		AmmoPickupInfo{ ID_PISTOLS_AMMO_ITEM, LaraWeaponType::Pistol, WeaponAmmoType::Ammo1, 30 },
		AmmoPickupInfo{ ID_UZI_AMMO_ITEM, LaraWeaponType::Uzi, WeaponAmmoType::Ammo1, 30 },
		AmmoPickupInfo{ ID_SHOTGUN_AMMO1_ITEM, LaraWeaponType::Shotgun, WeaponAmmoType::Ammo1, 6 },
		AmmoPickupInfo{ ID_SHOTGUN_AMMO2_ITEM, LaraWeaponType::Shotgun, WeaponAmmoType::Ammo2, 6 },
		AmmoPickupInfo{ ID_CROSSBOW_AMMO1_ITEM, LaraWeaponType::Crossbow, WeaponAmmoType::Ammo1, 10 },
		AmmoPickupInfo{ ID_CROSSBOW_AMMO2_ITEM, LaraWeaponType::Crossbow, WeaponAmmoType::Ammo2, 10 },
		AmmoPickupInfo{ ID_CROSSBOW_AMMO3_ITEM, LaraWeaponType::Crossbow, WeaponAmmoType::Ammo3, 10 },
		AmmoPickupInfo{ ID_REVOLVER_AMMO_ITEM, LaraWeaponType::Revolver, WeaponAmmoType::Ammo1, 6 },
		AmmoPickupInfo{ ID_HK_AMMO_ITEM, LaraWeaponType::HK, WeaponAmmoType::Ammo1, 30 },
		AmmoPickupInfo{ ID_GRENADE_AMMO1_ITEM, LaraWeaponType::GrenadeLauncher, WeaponAmmoType::Ammo1, 10 },
		AmmoPickupInfo{ ID_GRENADE_AMMO2_ITEM, LaraWeaponType::GrenadeLauncher, WeaponAmmoType::Ammo2, 10 },
		AmmoPickupInfo{ ID_GRENADE_AMMO3_ITEM, LaraWeaponType::GrenadeLauncher, WeaponAmmoType::Ammo3, 10 },
		AmmoPickupInfo{ ID_ROCKET_LAUNCHER_AMMO_ITEM, LaraWeaponType::RocketLauncher, WeaponAmmoType::Ammo1, 1 },
		AmmoPickupInfo{ ID_HARPOON_AMMO_ITEM, LaraWeaponType::HarpoonGun, WeaponAmmoType::Ammo1, 10 }
	}
};

void InitializeAmmo(const Settings& settings)
{
	for (auto& entry : Ammo)
		entry.Amount = settings.Weapons[(int)entry.LaraWeaponType - 1].PickupCount;
}

bool TryModifyingAmmo(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount, ModificationType modType)
{
	int arrayPos = GetArraySlot(Ammo, objectID);
	if (arrayPos == NO_VALUE)
		return false;

	const auto& ammoPickup = Ammo[arrayPos];

	auto& currentWeapon = lara.Weapons[(int)ammoPickup.LaraWeaponType];
	auto& currentAmmo = currentWeapon.Ammo[(int)ammoPickup.AmmoType];

	switch(modType)
	{
	case ModificationType::Set:
		currentAmmo = amount.value();
		currentAmmo.SetInfinite(amount == NO_VALUE);
		break;

	default:
		if (!currentAmmo.HasInfinite())
		{
			int defaultModify = modType == ModificationType::Add ? ammoPickup.Amount : -ammoPickup.Amount;
			int newVal = (int)currentAmmo.GetCount() + (amount.has_value() ? amount.value() : defaultModify);
			currentAmmo = std::max(0, newVal);
		}

		break;
	};

	return true;
}

bool TryAddingAmmo(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount)
{
	return TryModifyingAmmo(lara, objectID, amount, ModificationType::Add);
}

bool TryRemovingAmmo(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount)
{	
	if (amount.has_value())
	{
		return TryModifyingAmmo(lara, objectID, -amount.value(), ModificationType::Remove);
	}
	else
	{
		return TryModifyingAmmo(lara, objectID, amount, ModificationType::Remove);
	}
}

std::optional<int> GetAmmoCount(LaraInfo& lara, GAME_OBJECT_ID objectID)
{
	int arrayPos = GetArraySlot(Ammo, objectID);
	if (arrayPos == NO_VALUE)
		return std::nullopt;

	const auto& ammo = Ammo[arrayPos];

	if (!lara.Weapons[(int)ammo.LaraWeaponType].Ammo[(int)ammo.AmmoType].HasInfinite())
		return lara.Weapons[(int)ammo.LaraWeaponType].Ammo[(int)ammo.AmmoType].GetCount();

	// NO_VALUE signifies infinite ammo.
	return NO_VALUE;
}

int GetDefaultAmmoCount(GAME_OBJECT_ID objectID)
{
	int arrayPos = GetArraySlot(Ammo, objectID);
	if (arrayPos == NO_VALUE)
		return NO_VALUE;

	return Ammo[arrayPos].Amount;
}
