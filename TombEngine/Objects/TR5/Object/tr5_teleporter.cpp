#include "framework.h"
#include "Objects/TR5/Object/tr5_teleporter.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Environment;

void ControlTeleporter(short itemNumber)
{
	ItemInfo* item = &g_Level.Items[itemNumber];

	if (!TriggerActive(item))
		return;

	Lara.Control.IsLocked = false;

	if (item->TriggerFlags == 666)
	{
		if (item->ItemFlags[0] == 70)
		{
			SoundEffect(SFX_TR5_LIFT_HIT_FLOOR1, nullptr);
			SoundEffect(SFX_TR5_LIFT_HIT_FLOOR2, nullptr);
		}

		SetAnimation(*LaraItem, LA_ELEVATOR_RECOVER);

		item->ItemFlags[0]++;
		if (item->ItemFlags[0] >= 150)
			KillItem(itemNumber);
	}
	else
	{
		Camera.fixedCamera = true;
		LaraItem->DisableInterpolation = true;
		LaraItem->Pose.Position.x = item->Pose.Position.x;
		LaraItem->Pose.Position.z = item->Pose.Position.z;
		LaraItem->Pose.Orientation.y = item->Pose.Orientation.y - ANGLE(180.0f);

		auto& pointColl = GetPointCollision(*item);
		LaraItem->Pose.Position.y = pointColl.GetPosition().y;

		if (LaraItem->RoomNumber != pointColl.GetRoomNumber())
		{
			ItemNewRoom(LaraItem->Index, pointColl.GetRoomNumber());
			LaraItem->Location.RoomNumber = pointColl.GetRoomNumber();
		}

		if (item->Flags & IFLAG_INVISIBLE)
		{
			KillItem(itemNumber);
		}
		else if (item->TriggerFlags != 512)
		{
			RemoveActiveItem(itemNumber);
			item->Flags &= 0xC1FFu;
		}
	}
}
