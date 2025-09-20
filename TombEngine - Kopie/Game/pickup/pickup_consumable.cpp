#include "framework.h"
#include "Game/pickup/pickup_consumable.h"

#include <array>

#include "Game/Lara/lara_struct.h"
#include "Game/pickup/pickuputil.h"
#include "Objects/objectslist.h"

struct ConsumablePickupInfo
{
	GAME_OBJECT_ID ObjectID;
	// Pointer to array of consumable in question
	int PlayerInventoryData::* Count;
	// How many of the item to give the player if the caller
	// does not specify; i.e. default amount per pickup
	int Amount;
};

static std::array<ConsumablePickupInfo, 4> Consumables =
{
	{
		ConsumablePickupInfo{ ID_SMALLMEDI_ITEM, &PlayerInventoryData::TotalSmallMedipacks, 1 },
		ConsumablePickupInfo{ ID_BIGMEDI_ITEM, &PlayerInventoryData::TotalLargeMedipacks, 1 },
		ConsumablePickupInfo{ ID_FLARE_INV_ITEM, &PlayerInventoryData::TotalFlares, 12 },
		ConsumablePickupInfo{ ID_FLARE_ITEM, &PlayerInventoryData::TotalFlares, 1 }
	}
 };

void InitializeConsumables(const Settings& settings)
{
	Consumables[GetArraySlot(Consumables, GAME_OBJECT_ID::ID_FLARE_INV_ITEM)].Amount = settings.Flare.PickupCount;
}

bool TryModifyingConsumable(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount, ModificationType modType)
{
	int arrayPos = GetArraySlot(Consumables, objectID);
	if (arrayPos == NO_VALUE)
		return false;

	const auto& consumable = Consumables[arrayPos];
	auto& currentAmount = lara.Inventory.*(consumable.Count);
	switch (modType)
	{
	case ModificationType::Set:
		currentAmount = amount.value();
		break;

	default:
		if (currentAmount != NO_VALUE)
		{
			int defaultModify = ModificationType::Add == modType ? consumable.Amount : -consumable.Amount;
			int newVal = currentAmount + (amount.has_value() ? amount.value() : defaultModify);
			currentAmount = std::max(0, newVal);
		}
		break;
	}

	return true;
}

bool TryAddingConsumable(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount)
{
	return TryModifyingConsumable(lara, objectID, amount, ModificationType::Add);
}

bool TryRemovingConsumable(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount)
{
	if (amount.has_value())
	{
		return TryModifyingConsumable(lara, objectID, -amount.value(), ModificationType::Remove);
	}
	else
	{
		return TryModifyingConsumable(lara, objectID, amount, ModificationType::Remove);
	}
}

std::optional<int> GetConsumableCount(LaraInfo& lara, GAME_OBJECT_ID objectID)
{
	int arrayPos = GetArraySlot(Consumables, objectID);
	if (arrayPos == NO_VALUE)
		return std::nullopt;

	const auto& consumable = Consumables[arrayPos];

	return lara.Inventory.*(consumable.Count);
}

int GetDefaultConsumableCount(GAME_OBJECT_ID objectID)
{
	int arrayPos = GetArraySlot(Consumables, objectID);
	if (arrayPos == NO_VALUE)
		return NO_VALUE;

	return Consumables[arrayPos].Amount;
}
