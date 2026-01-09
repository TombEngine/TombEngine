#include "framework.h"
#include "Objects/TR4/Trap/tr4_birdblade.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Traps
{
	void InitializeBirdBlade(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[4] = 1;
	}

	void ControlBirdBlade(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[3] = 100;

		if (!TriggerActive(&item))
		{
			item.Animation.FrameNumber = 0;
			*((int*)&item.ItemFlags[0]) = 0;
		}
		else
		{
			int frameNumber = item.Animation.FrameNumber;

			if (frameNumber <= 14 || frameNumber >= 31)
			{
				*((int*)&item.ItemFlags[0]) = 0;
			}
			else
			{
				*((int*)&item.ItemFlags[0]) = 6;
			}

			AnimateItem(item);
		}
	}
}
