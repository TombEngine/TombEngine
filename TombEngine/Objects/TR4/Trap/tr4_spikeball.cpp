#include "framework.h"
#include "Objects/TR4/Trap/tr4_spikeball.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Traps
{
	void ControlSpikeball(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			int frameNumber = item.Animation.FrameNumber;

			if ((frameNumber <= 14 || frameNumber >= 24) &&
				(frameNumber < 138 || frameNumber > 140))
			{
				if (frameNumber < 141)
				{
					*((int*)&item.ItemFlags[0]) = 0;
				}
				else
				{
					item.ItemFlags[3] = 50;
					*((int*)&item.ItemFlags[0]) = 0x7FF800;
				}
			}
			else
			{
				item.ItemFlags[3] = 150;
				*((int*)&item.ItemFlags[0]) = 0x7FF800;
			}

			AnimateItem(item);
		}
		else
		{
			item.Animation.FrameNumber = 0;
			*((int*)&item.ItemFlags[0]) = 0;
		}
	}
}
