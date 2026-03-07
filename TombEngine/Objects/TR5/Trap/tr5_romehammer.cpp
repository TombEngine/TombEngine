#include "framework.h"
#include "Objects/TR5/Trap/tr5_romehammer.h"

#include "Game/items.h"
#include "Specific/level.h"

// NOTES:
// item.ItemFlags[0] = Harm joints.
// item.ItemFlags[3] = Damage.
// item.ItemFlags[4] = Push player (bool).

namespace TEN::Entities::Traps
{
	constexpr auto ROME_HAMMER_HARM_DAMAGE = 250;
	constexpr auto ROME_HAMMER_JOINTS	   = MESH_BITS(1);

	const auto RomeHammerHarmJoints = std::vector<unsigned int>{ 2 };

	void InitializeRomeHammer(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[0] = ROME_HAMMER_JOINTS;
		item.ItemFlags[3] = 0;
	}

	void ControlRomeHammer(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			item.ItemFlags[3] = ROME_HAMMER_HARM_DAMAGE;
			AnimateItem(item);
		}
		else
		{
			item.ItemFlags[3] = 0;
		}
	}
}
