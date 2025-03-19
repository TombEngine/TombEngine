#include "framework.h"
#include "Objects/TR5/Trap/tr5_fallingceiling.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;

namespace TEN::Entities::Traps
{
	constexpr auto FALLING_CEILING_HARM_DAMAGE = 300;

	void ControlFallingCeiling(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Animation.ActiveState)
		{
			if (item.Animation.ActiveState == 1 && item.TouchBits.TestAny())
				DoDamage(LaraItem, FALLING_CEILING_HARM_DAMAGE);
		}
		else
		{
			item.Animation.TargetState = 1;
			item.Animation.IsAirborne = true;
		}

		AnimateItem(item);

		if (item.Status == ITEM_DEACTIVATED)
		{
			RemoveActiveItem(itemNumber);
		}
		else
		{
			auto pointColl = GetPointCollision(item);

			item.Floor = pointColl.GetFloorHeight();

			if (pointColl.GetRoomNumber() != item.RoomNumber)
				ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

			if (item.Animation.ActiveState == 1)
			{
				if (item.Pose.Position.y >= item.Floor)
				{
					item.Pose.Position.y = item.Floor;
					item.Animation.TargetState = 2;
					item.Animation.IsAirborne = false;
					item.Animation.Velocity.y = 0.0f;
				}
			}
		}
	}
}
