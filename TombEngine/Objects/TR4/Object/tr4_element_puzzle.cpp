#include "framework.h"
#include "tr4_element_puzzle.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;
using namespace TEN::Hud;
using namespace TEN::Input;
using namespace TEN::Entities::Switches;

namespace TEN::Entities::TR4
{
	void InitializeElementPuzzle(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->TriggerFlags)
		{
			if (item->TriggerFlags == 1)
				item->MeshBits = 65;
			else if (item->TriggerFlags == 2)
				item->MeshBits = 68;
			else
				item->MeshBits = 0;
		}
		else
			item->MeshBits = 80;
	}
}
