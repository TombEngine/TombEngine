#include "framework.h"
#include "Objects/TR4/Trap/tr4_plinthblade.h"

#include "Game/animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Specific/level.h"

namespace TEN::Entities::Traps
{
	void ControlPlinthBlade(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
		{
			item.Animation.FrameNumber = GetAnimData(item).frameBase;
		}
		else
		{
			int frameNumber = item.Animation.FrameNumber - GetAnimData(item).frameBase;

			if (item.Animation.FrameNumber == GetAnimData(item).frameEnd)
			{
				item.ItemFlags[3] = 0;
			}
			else
			{
				item.ItemFlags[3] = 200;
			}

			AnimateItem(&item);
		}
	}
}
