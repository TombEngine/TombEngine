#include "framework.h"
#include "Objects/TR4/Entity/tr4_lara_double.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::TR4
{
	void LaraDoubleControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		SoundEffect(SFX_TR4_METAL_SCRAPE_LOOP1, &item->Pose);

		if (CreatureActive(itemNumber))
		{
			if (item->HitStatus)
				LaraItem->HitPoints = item->HitPoints;

			AnimateItem(*item);
		}
	}
}
