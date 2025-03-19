#include "framework.h"
#include "Objects/TR4/Trap/tr4_plinthblade.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Traps
{
	void ControlPlinthBlade(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
		{
			item.Animation.FrameNumber = 0;
		}
		else
		{
			int frameNumber = item.Animation.FrameNumber;

			if (TestLastFrame(item))
			{
				item.ItemFlags[3] = 0;
			}
			else
			{
				item.ItemFlags[3] = 200;
			}

			AnimateItem(item);
		}
	}
}
