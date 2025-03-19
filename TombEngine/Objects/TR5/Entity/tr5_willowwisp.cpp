#include "framework.h"
#include "Objects/TR5/Entity/tr5_willowwisp.h"

#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Specific/level.h"

namespace TEN::Entities::Creatures::TR5
{
	enum WillowWispState
	{
		WWISP_STATE_UNK = 1
	};

	void InitializeLightingGuide(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 0);
	}
}
