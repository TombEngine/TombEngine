#include "framework.h"
#include "Game/pickup/pickup_misc_items.h"

#include "Game/Lara/lara_struct.h"
#include "Game/pickup/pickuputil.h"
#include "Objects/objectslist.h"

auto SilencerIsEquipped(LaraInfo& lara)
{
	if (lara.Weapons[(int)LaraWeaponType::Uzi].HasSilencer ||
		lara.Weapons[(int)LaraWeaponType::Pistol].HasSilencer ||
		lara.Weapons[(int)LaraWeaponType::Shotgun].HasSilencer ||
		lara.Weapons[(int)LaraWeaponType::Revolver].HasSilencer ||
		lara.Weapons[(int)LaraWeaponType::Crossbow].HasSilencer ||
		lara.Weapons[(int)LaraWeaponType::HK].HasSilencer)
	{
		return true;
	}

	return false;
};

auto LaserSightIsEquipped(LaraInfo& lara)
{
	if (lara.Weapons[(int)LaraWeaponType::Revolver].HasLasersight ||
		lara.Weapons[(int)LaraWeaponType::Crossbow].HasLasersight ||
		lara.Weapons[(int)LaraWeaponType::HK].HasLasersight)
	{
		return true;
	}

	return false;
};

bool TryModifyMiscCount(LaraInfo & lara, GAME_OBJECT_ID objectID, std::optional<int> amount, ModificationType modType)
{
	// If adding, replace the small/large waterskin with one of the requested
	// capacity. If removing, only remove the waterskin if it contains the given
	// capacity.

	bool add = ModificationType::Add == modType || ((ModificationType::Set == modType) && amount != 0);

	auto modifyWaterSkinAmount = [&](byte& currentFlag, byte newFlag)
	{
		if (add)
			currentFlag = newFlag;
		else
		{
			if (currentFlag == newFlag)
				currentFlag = 0;
		}
	};

	auto modifyBeetleCount = [&](int bit)
	{
		if (add)
			lara.Inventory.BeetleComponents |= 1 << bit;
		else
			lara.Inventory.BeetleComponents &= ~(1 << bit);
	};

	switch (objectID)
	{
	case ID_SILENCER_ITEM:
		lara.Inventory.HasSilencer = add && !SilencerIsEquipped(lara);
		break;

	case ID_LASERSIGHT_ITEM:
		lara.Inventory.HasLasersight = add && !LaserSightIsEquipped(lara);
		break;

	case ID_BINOCULARS_ITEM:
		lara.Inventory.HasBinoculars = add;
		break;

	case ID_CROWBAR_ITEM:
		lara.Inventory.HasCrowbar = add;
		break;

	case ID_DIARY_ITEM:
		lara.Inventory.HasDiary = add;
		break;

	case ID_PC_LOAD_INV_ITEM:
		lara.Inventory.HasLoad = add;
		break;

	case ID_PC_SAVE_INV_ITEM:
		lara.Inventory.HasSave = add;
		break;

	case ID_STOPWATCH_ITEM:
		lara.Inventory.HasStopwatch = add;
		break;

	case ID_COMPASS_ITEM:
		lara.Inventory.HasCompass = add;
		break;

	case ID_WATERSKIN1_EMPTY:
		modifyWaterSkinAmount(lara.Inventory.SmallWaterskin, 1);
		break;

	case ID_WATERSKIN1_1:
		modifyWaterSkinAmount(lara.Inventory.SmallWaterskin, 2);
		break;

	case ID_WATERSKIN1_2:
		modifyWaterSkinAmount(lara.Inventory.SmallWaterskin, 3);
		break;

	case ID_WATERSKIN1_3:
		modifyWaterSkinAmount(lara.Inventory.SmallWaterskin, 4);
		break;

	case ID_WATERSKIN2_EMPTY:
		modifyWaterSkinAmount(lara.Inventory.BigWaterskin, 1);
		break;

	case ID_WATERSKIN2_1:
		modifyWaterSkinAmount(lara.Inventory.BigWaterskin, 2);
		break;

	case ID_WATERSKIN2_2:
		modifyWaterSkinAmount(lara.Inventory.BigWaterskin, 3);
		break;

	case ID_WATERSKIN2_3:
		modifyWaterSkinAmount(lara.Inventory.BigWaterskin, 4);
		break;

	case ID_WATERSKIN2_4:
		modifyWaterSkinAmount(lara.Inventory.BigWaterskin, 5);
		break;

	case ID_WATERSKIN2_5:
		modifyWaterSkinAmount(lara.Inventory.BigWaterskin, 6);
		break;

	case ID_CLOCKWORK_BEETLE:
		modifyBeetleCount(0);
		break;

	case ID_CLOCKWORK_BEETLE_COMBO1:
		modifyBeetleCount(1);
		break;

	case ID_CLOCKWORK_BEETLE_COMBO2:
		modifyBeetleCount(2);
		break;

	default:
		return false;
	}

	return true;
}

bool TryAddMiscItem(LaraInfo& lara, GAME_OBJECT_ID objectID)
{
	return TryModifyMiscCount(lara, objectID, std::nullopt, ModificationType::Add);
}

bool TryRemoveMiscItem(LaraInfo& lara, GAME_OBJECT_ID objectID)
{
	return TryModifyMiscCount(lara, objectID, std::nullopt, ModificationType::Remove);
}

std::optional<bool> HasMiscItem(LaraInfo& lara, GAME_OBJECT_ID objectID)
{	
	auto HasBeetle = [&](int bit)
	{
		return lara.Inventory.BeetleComponents &= 1 << bit;
	};

	switch (objectID)
	{
		//TODO does Lara "HAVE" a silencer if it's combined but not in her inventory?
	case ID_SILENCER_ITEM:
		return lara.Inventory.HasSilencer || SilencerIsEquipped(lara);

	case ID_LASERSIGHT_ITEM:
		return lara.Inventory.HasLasersight || LaserSightIsEquipped(lara);

	case ID_BINOCULARS_ITEM:
		return lara.Inventory.HasBinoculars;

	case ID_CROWBAR_ITEM:
		return lara.Inventory.HasCrowbar;

	case ID_DIARY_ITEM:
		return lara.Inventory.HasDiary;

	case ID_PC_LOAD_INV_ITEM:
		return lara.Inventory.HasLoad;

	case ID_PC_SAVE_INV_ITEM:
		return lara.Inventory.HasSave;

	case ID_STOPWATCH_ITEM:
		return lara.Inventory.HasStopwatch;

	case ID_COMPASS_ITEM:
		return lara.Inventory.HasCompass;

	case ID_WATERSKIN1_EMPTY:
		return lara.Inventory.SmallWaterskin == 1;

	case ID_WATERSKIN1_1:
		return lara.Inventory.SmallWaterskin == 2;

	case ID_WATERSKIN1_2:
		return lara.Inventory.SmallWaterskin == 3;

	case ID_WATERSKIN1_3:
		return lara.Inventory.SmallWaterskin == 4;

	case ID_WATERSKIN2_EMPTY:
		return lara.Inventory.BigWaterskin == 1;

	case ID_WATERSKIN2_1:
		return lara.Inventory.BigWaterskin == 2;

	case ID_WATERSKIN2_2:
		return lara.Inventory.BigWaterskin == 3;

	case ID_WATERSKIN2_3:
		return lara.Inventory.BigWaterskin == 4;

	case ID_WATERSKIN2_4:
		return lara.Inventory.BigWaterskin == 5;

	case ID_WATERSKIN2_5:
		return lara.Inventory.BigWaterskin == 6;

	case ID_CLOCKWORK_BEETLE:
		return HasBeetle(0);

	case ID_CLOCKWORK_BEETLE_COMBO1:
		return HasBeetle(1);

	case ID_CLOCKWORK_BEETLE_COMBO2:
		return HasBeetle(2);

	default:
		return std::nullopt;
	}
}
