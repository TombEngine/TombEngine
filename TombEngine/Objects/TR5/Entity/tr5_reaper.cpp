#include "framework.h"
#include "Objects/TR5/Entity/tr5_reaper.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR5
{
	void InitializeReaper(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 1);
	}

	void ReaperControl(short itemNumber)
	{
		if (CreatureActive(itemNumber))
		{
			auto* item = &g_Level.Items[itemNumber];
			auto* creature = GetCreatureInfo(item);

			if (item->AIBits)
				GetAITarget(creature);
			else
				creature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			GetCreatureMood(item, &AI, false);
			CreatureMood(item, &AI, false);

			short angle = CreatureTurn(item, ANGLE(2.0f));

			if (item->Animation.ActiveState == 2 &&
				!(GetRandomControl() & 0x3F))
				item->Animation.TargetState = 1;

			if (creature->ReachedGoal)
			{
				if (creature->Enemy)
				{
					if (creature->Enemy->Flags & 2)
						item->ItemFlags[3] = (creature->Tosspad & 0xFF) - 1;

					item->ItemFlags[3]++;

					creature->ReachedGoal = false;
					creature->Enemy = nullptr;
				}
			}

			item->Pose.Orientation.x = -ANGLE(67.5f);
			CreatureAnimation(itemNumber, angle, 0);
			CreatureUnderwater(item, BLOCK(1));
		}
	}
}
