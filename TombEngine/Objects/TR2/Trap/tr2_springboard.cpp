#include "framework.h"
#include "Objects/TR2/Trap/tr2_springboard.h"

#include "Game/Animation/Animation.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Specific/level.h"

using namespace TEN::Animation;

void SpringBoardControl(short itemNumber)
{
	auto& item = g_Level.Items[itemNumber];

	if (item.Animation.ActiveState == 0 && LaraItem->Pose.Position.y == item.Pose.Position.y &&
		(LaraItem->Pose.Position.x / BLOCK(1)) == (item.Pose.Position.x / BLOCK(1)) &&
		(LaraItem->Pose.Position.z / BLOCK(1)) == (item.Pose.Position.z / BLOCK(1)))
	{
		if (LaraItem->HitPoints <= 0)
			return;

		if (LaraItem->Animation.ActiveState == LS_WALK_BACK ||
			LaraItem->Animation.ActiveState == LS_RUN_BACK)
		{
			LaraItem->Animation.Velocity.z *= -1;
		}

		SetAnimation(*LaraItem, LA_FALL_START);
		LaraItem->Animation.IsAirborne = true;
		LaraItem->Animation.Velocity.y = -240.0f;

		item.Animation.TargetState = 1;
	}

	AnimateItem(item);
}
