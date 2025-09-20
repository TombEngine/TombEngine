#include "framework.h"
#include "Game/pickup/pickup_key_items.h"

#include "Game/Lara/lara_struct.h"
#include "Game/pickup/pickuputil.h"
#include "Objects/objectslist.h"

template <size_t N> struct KeyPickupInfo
{
	// the array in the LaraInfo struct that holds the amount of
	// each type of key item (puzzle, key, examine, combos, etc)
	int(PlayerInventoryData::* arr)[N];
	// The range of GAME_OBJECT_IDs that correspond to each type
	// of key item
	// ID of first and last objects of this type
	GAME_OBJECT_ID min;
	GAME_OBJECT_ID max;
	constexpr KeyPickupInfo<N>(int(PlayerInventoryData::* a_array)[N], GAME_OBJECT_ID a_min, GAME_OBJECT_ID a_max) : arr{ a_array }, min{ a_min }, max{ a_max } {};
};

static constexpr int kDefaultPickupAmount = 1;

// This has to be a tuple since the pointers-to-arrays all have different types
// due to differing array sizes.
static constexpr std::tuple kKeyPickupInfos = std::make_tuple( 
	KeyPickupInfo { &PlayerInventoryData::Puzzles, ID_PUZZLE_ITEM1, ID_PUZZLE_ITEM16 },
	KeyPickupInfo { &PlayerInventoryData::PuzzlesCombo, ID_PUZZLE_ITEM1_COMBO1, ID_PUZZLE_ITEM16_COMBO2 },
	KeyPickupInfo { &PlayerInventoryData::Keys, ID_KEY_ITEM1, ID_KEY_ITEM16 },
	KeyPickupInfo { &PlayerInventoryData::KeysCombo, ID_KEY_ITEM1_COMBO1, ID_KEY_ITEM16_COMBO2 },
	KeyPickupInfo { &PlayerInventoryData::Pickups, ID_PICKUP_ITEM1, ID_PICKUP_ITEM16 },
	KeyPickupInfo { &PlayerInventoryData::PickupsCombo, ID_PICKUP_ITEM1_COMBO1, ID_PICKUP_ITEM16_COMBO2 },
	KeyPickupInfo { &PlayerInventoryData::Examines, ID_EXAMINE1, ID_EXAMINE8 },
	KeyPickupInfo { &PlayerInventoryData::ExaminesCombo, ID_EXAMINE1_COMBO1, ID_EXAMINE8_COMBO2 });

static constexpr auto nInfos = std::tuple_size<decltype(kKeyPickupInfos)>{};

// Test against min and max of kKeyPickupInfos[N].
// if found, return the pointer to the LaraInfo array
// as well as the position within that array.
template<size_t N> static constexpr std::pair<int*, size_t> TestAgainstRange(LaraInfo& lara, GAME_OBJECT_ID objectID)
{
	auto pickupInfo = std::get<N>(kKeyPickupInfos);
	int min = pickupInfo.min;
	int max = pickupInfo.max;

	if (objectID >= min && objectID <= max)
		return std::make_pair(lara.Inventory.*(pickupInfo.arr), objectID - min);
	
	return std::make_pair(nullptr, 0);
}

// Test against each kKeyPickupInfos item.
// This is recursive because I couldn't find a simpler
// way to iterate across a tuple.
template<size_t N> static std::pair<int*, size_t> GetArrayInternal(LaraInfo& lara, GAME_OBJECT_ID objectID)
{
	auto h = TestAgainstRange<N>(lara, objectID);
	return h.first ? h : GetArrayInternal<N-1>(lara, objectID);
}

// Base case for recursion
template<> static std::pair<int*, size_t> GetArrayInternal<0>(LaraInfo& lara, GAME_OBJECT_ID objectID)
{
	return TestAgainstRange<0>(lara, objectID);
}

bool TryModifyingKeyItem(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount, ModificationType modType)
{
	// kick off the recursion starting at the last element
	auto result = GetArrayInternal<nInfos - 1>(lara, objectID);

	if (result.first)
	{
		auto& amt = result.first[result.second];
		switch (modType)
		{
		case ModificationType::Set:
			// infinite key items not yet implemented
			amt = amount.value();
			break;
		default:
			int defaultModify = modType == ModificationType::Add ? kDefaultPickupAmount : -kDefaultPickupAmount;
			int newVal = int{ result.first[result.second] } + (amount.has_value() ? amount.value() : defaultModify);
			result.first[result.second] = std::max(0, newVal);
		}
		return true;
	}

	return false;
}

bool TryAddingKeyItem(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount)
{
	return TryModifyingKeyItem(lara, objectID, amount, ModificationType::Add);
}

bool TryRemovingKeyItem(LaraInfo& lara, GAME_OBJECT_ID objectID, std::optional<int> amount)
{
	if(amount.has_value())
		return TryModifyingKeyItem(lara, objectID, -amount.value(), ModificationType::Remove);
	else
		return TryModifyingKeyItem(lara, objectID, amount, ModificationType::Remove);
}

std::optional<int> GetKeyItemCount(LaraInfo& lara, GAME_OBJECT_ID objectID)
{
	// kick off the recursion starting at the last element
	auto result = GetArrayInternal<nInfos - 1>(lara, objectID);
	if (result.first)
		return result.first[result.second];

	return std::nullopt;
}
