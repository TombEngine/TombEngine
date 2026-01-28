#include "framework.h"
#include "Objects/TR4/Trap/tr4_chain.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Traps
{
	void ControlChain(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.TriggerFlags)
		{
			item.ItemFlags[2] = 1;
			item.ItemFlags[3] = 75;

			if (TriggerActive(&item))
			{
				*(int*)&item.ItemFlags[0] = 0x787E;
				AnimateItem(item);
				return;
			}
		}
		else
		{
			item.ItemFlags[3] = 25;

			if (TriggerActive(&item))
			{
				*(int*)&item.ItemFlags[0] = 0x780;
				AnimateItem(item);
				return;
			}
		}

		*(int*)&item.ItemFlags[0] = 0;
	}
}
