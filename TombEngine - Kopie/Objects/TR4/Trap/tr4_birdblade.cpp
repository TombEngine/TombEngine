#include "framework.h"
#include "Objects/TR4/Trap/tr4_birdblade.h"

#include "Game/animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Specific/level.h"

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
			item.Animation.FrameNumber = GetAnimData(item).frameBase;
			*((int*)&item.ItemFlags[0]) = 0;
		}
		else
		{
			int frameNumber = item.Animation.FrameNumber - GetAnimData(item).frameBase;

			if (frameNumber <= 14 || frameNumber >= 31)
			{
				*((int*)&item.ItemFlags[0]) = 0;
			}
			else
			{
				*((int*)&item.ItemFlags[0]) = 6;
			}

			AnimateItem(&item);
		}
	}
}
