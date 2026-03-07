#include "framework.h"
#include "Objects/TR2/Trap/tr2_killerstatue.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Traps
{
	constexpr auto KILLER_STATUE_HARM_DAMAGE = 200;

	void InitializeKillerStatue(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.Animation.AnimNumber = 3;
		item.Animation.FrameNumber = 0;
		item.Animation.ActiveState = 1;
	}

	void ControlKillerStatue(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item) && item.Animation.ActiveState == 1)
		{
			item.Animation.TargetState = 2;
		}
		else
		{
			item.Animation.TargetState = 1;
		}

		if (item.TouchBits & 0x80 && item.Animation.ActiveState == 2)
		{
			DoDamage(LaraItem, KILLER_STATUE_HARM_DAMAGE);

			int x = LaraItem->Pose.Position.x + (GetRandomControl() - BLOCK(16)) / CLICK(1);
			int z = LaraItem->Pose.Position.z + (GetRandomControl() - BLOCK(16)) / CLICK(1);
			int y = LaraItem->Pose.Position.y - GetRandomControl() / 44;
			int d = (GetRandomControl() - BLOCK(16)) / 8 + LaraItem->Pose.Orientation.y;

			DoBloodSplat(x, y, z, LaraItem->Animation.Velocity.z, d, LaraItem->RoomNumber);
		}

		AnimateItem(item);
	}
}
