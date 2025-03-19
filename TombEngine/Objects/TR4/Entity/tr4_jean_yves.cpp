#include "framework.h"
#include "Objects/TR4/Entity/tr4_jean_yves.h"

#include "Game/control/control.h"
#include "Game/Animation/Animation.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	enum JeanYvesState
	{
		// No state 0.
		JEAN_YVES_STATE_HANDS_BEHIND_HEAD = 1,
		JEAN_YVES_STATE_THINKING1 = 2,
		JEAN_YVES_STATE_THINKING2 = 3,
		JEAN_YVES_STATE_THINKING3 = 4,
		JEAN_YVES_STATE_INSPECT_BOOKSHELF_RIGHT1 = 5,
		JEAN_YVES_STATE_INSPECT_BOOKSHELF_RIGHT2 = 6,
		JEAN_YVES_STATE_INSPECT_BOOKSHELF_RIGHT3 = 7,
		JEAN_YVES_STATE_INSPECT_BOOKSHELF_RIGHT_BELOW = 8,
	};

	enum JeanYvesAnim
	{

	};

	void InitializeJeanYves(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		auto* objectInfo = &Objects[item->ObjectNumber];

		item->Animation.TargetState = JEAN_YVES_STATE_HANDS_BEHIND_HEAD;
		item->Animation.ActiveState = JEAN_YVES_STATE_HANDS_BEHIND_HEAD;
		item->Animation.AnimNumber = 0;
		item->Animation.FrameNumber = 0;
	}

	void JeanYvesControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->TriggerFlags >= Lara.HighestLocation)
		{
			int state = 0;

			if (Random::TestProbability(0.75f))
				state = (GetRandomControl() & 1) + 1;
			else
				state = 3 * (GetRandomControl() & 1);

			item->Animation.TargetState = (((byte)(item->Animation.ActiveState) - 1) & 0xC) + state + 1;
			AnimateItem(*item);
		}
		else
		{
			if (Lara.HighestLocation > 3)
				Lara.HighestLocation = 3;

			int state = (GetRandomControl() & 3) + 4 * Lara.HighestLocation;
			int animNumber = state;
			state++;

			item->Animation.AnimNumber = animNumber;
			item->Animation.FrameNumber = 0;
			item->Animation.ActiveState = state;
			item->Animation.TargetState = state;
			item->TriggerFlags = Lara.HighestLocation;

			AnimateItem(*item);
		}
	}
}
