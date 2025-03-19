#include "framework.h"
#include "Objects/TR4/Object/WraithTrap.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/spark.h"
#include "Game/items.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Game/Setup.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Spark;
using namespace TEN::Input;

namespace TEN::Entities::TR4
{
	void InitializeWraithTrap(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		SetAnimation(item, 0);
		item.ItemFlags[6] = 0;
	}

	void WraithTrapControl(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			if (item.ItemFlags[6] && item.TriggerFlags > 0)
			{
				auto pos = GetJointPosition(&item, 0, Vector3i::Zero);

				SoundEffect(SFX_TR4_LIGHT_BEAM_LOOP, &Pose(pos));

				auto color = Vector3(255.0f);
				TriggerAttackSpark(pos.ToVector3(), color);
			}
		}
	}
}
