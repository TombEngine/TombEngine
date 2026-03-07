#include "framework.h"
#include "Objects/TR1/Trap/SlammingDoors.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Entities::Traps
{
	constexpr auto SLAMMING_DOORS_HARM_DAMAGE = 400;

	enum SlammingDoorsState
	{
		SLAMMING_DOORS_DISABLED = 0,
		SLAMMING_DOORS_ENABLED = 1
	};

	enum SlammingDoorsAnim
	{
		SLAMMING_DOORS_ANIM_OPENED = 0,
		SLAMMING_DOORS_ANIM_CLOSING = 1,
		SLAMMING_DOORS_ANIM_OPENING = 2
	};

	void InitializeSlammingDoors(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		SetAnimation(item, SLAMMING_DOORS_ANIM_OPENED);

		// Used by GenericSphereBoxCollision. Bits correspond to joint damage index.
		// 3 = 000000000 000000011, so damage joints are 1 and 2 (both doors).
		item.ItemFlags[0] = 0;

		// Used by GenericSphereBoxCollision for trap damage value.
		item.ItemFlags[3] = SLAMMING_DOORS_HARM_DAMAGE;
	}

	void ControlSlammingDoors(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			if (item.Animation.TargetState != SLAMMING_DOORS_ENABLED)
			{
				item.Animation.TargetState = SLAMMING_DOORS_ENABLED;
				item.ItemFlags[0] = 3;
			}
		}
		else
		{
			if (item.Animation.TargetState != SLAMMING_DOORS_DISABLED)
			{
				item.Animation.TargetState = SLAMMING_DOORS_DISABLED;
				item.ItemFlags[0] = 0;
			}
		}

		AnimateItem(item);
	}
}
