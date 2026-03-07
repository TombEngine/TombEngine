#include "framework.h"
#include "Objects/TR4/Trap/tr4_fourblades.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Specific/level.h"

using namespace TEN::Animation;

// NOTES:
// item.ItemFlags[0] = Damage joints.
// item.ItemFlags[3] = Damage.
// item.ItemFlags[4] = Push player (bool).

namespace TEN::Entities::Traps
{
	constexpr auto FOUR_BLADES_EMERGE_HARM_DAMAGE = 20;
	constexpr auto FOUR_BLADES_IDLE_HARM_DAMAGE	  = 200;

	constexpr auto FOUR_BLADES_HARM_JOINTS = MESH_BITS(1) | MESH_BITS(2) | MESH_BITS(3) | MESH_BITS(4);

	void InitializeFourBlades(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[4] = 1;
	}

	void ControlFourBlades(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
		{
			item.Animation.FrameNumber = 0;
			item.ItemFlags[0] = 0;
		}
		else
		{
			int frameNumber = item.Animation.FrameNumber;
			if (frameNumber <= 5 ||
				frameNumber >= 58 ||
				frameNumber >= 8 && frameNumber <= 54)
			{
				item.ItemFlags[0] = 0;
				item.ItemFlags[3] = 0;
			}
			else
			{
				item.ItemFlags[0] = FOUR_BLADES_HARM_JOINTS;

				if (frameNumber >= 6 && frameNumber <= 7)
				{
					item.ItemFlags[3] = FOUR_BLADES_EMERGE_HARM_DAMAGE;	
				}
				else if (frameNumber >= 55 && frameNumber <= 57)
				{
					item.ItemFlags[3] = FOUR_BLADES_IDLE_HARM_DAMAGE;
				}
			}

			AnimateItem(item);
		}
	}
}
