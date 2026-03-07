#include "framework.h"
#include "Objects/TR4/Trap/tr4_sethblade.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Traps
{
	constexpr auto SETH_BLADE_HARM_DAMAGE = 1000;

	enum SethBladeState 
	{
		// No state 0.
		SETHBLADE_STATE_ACTIVE = 1,
		SETHBLADE_STATE_IDLE = 2
	};

	enum SethBladeAnim 
	{
		SETHBLADE_ANIM_ACTIVATE = 0,
		SETHBLADE_ANIM_IDLE = 1
	};

	void InitializeSethBlade(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		SetAnimation(item, SETHBLADE_ANIM_IDLE);
		
		item.ItemFlags[2] = item.TriggerFlags >= 0 ? 1 : abs(item.TriggerFlags);		//ItemFlags[2] stores blade timer.
		item.ItemFlags[3] = SETH_BLADE_HARM_DAMAGE;										//ItemFlags[3] stored blade harm damage.
		item.ItemFlags[4] = 1;
	}

	void ControlSethBlade(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		*((int*)&item.ItemFlags) = 0;

		if (TriggerActive(&item))
		{
			if (item.Animation.ActiveState == SETHBLADE_STATE_IDLE)
			{
				if (item.ItemFlags[2] > 1)
				{
					item.ItemFlags[2]--;
				}
				else if (item.ItemFlags[2] == 1)
				{
					item.Animation.TargetState = SETHBLADE_STATE_ACTIVE;
					item.ItemFlags[2] = 0;
				}
				else if (item.ItemFlags[2] == 0)
				{
					if (item.TriggerFlags > 0)
						item.ItemFlags[2] = abs(item.TriggerFlags);
				}
			}
			else if (item.Animation.ActiveState == SETHBLADE_STATE_ACTIVE)
			{
				int frame = item.Animation.FrameNumber;

				if (frame >= 0 && frame <= 6)
				{
					*((int*)&item.ItemFlags) = -1;
				}
				else if (frame >= 7 && frame <= 15)
				{
					*((int*)&item.ItemFlags) = 448;
				}
				else
				{
					*((int*)&item.ItemFlags) = 0;
				}
			}

			AnimateItem(item);
		}
	}
}
