#include "framework.h"
#include "Objects/TR4/Trap/tr4_plough.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Traps
{
	void InitializePlough(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[4] = 1;
	}

	void ControlPlough(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[3] = 50;

		if (TriggerActive(&item))
		{
			*((int*)&item.ItemFlags) = 0x3F000;
			AnimateItem(item);
		}
		else
		{
			*((int*)&item.ItemFlags) = 0;
		}
	}
}
