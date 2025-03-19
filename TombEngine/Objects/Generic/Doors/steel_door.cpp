#include "framework.h"
#include "Objects/Generic/Doors/generic_doors.h"
#include "Specific/level.h"
#include "Game/control/control.h"
#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/control/lot.h"
#include "Game/Gui.h"
#include "Specific/Input/Input.h"
#include "Game/pickup/pickup.h"
#include "Sound/sound.h"
#include "Game/Animation/Animation.h"
#include "Game/collision/Sphere.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"
#include "Game/misc.h"
#include "Objects/Generic/Doors/steel_door.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/collide_item.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;

namespace TEN::Entities::Doors
{
	void InitializeSteelDoor(short itemNumber)
	{
		auto* doorItem = &g_Level.Items[itemNumber];

		doorItem->MeshBits = 1;
		doorItem->Pose.Position.y -= 1024;
	}

	void SteelDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* doorItem = &g_Level.Items[itemNumber];

		if (doorItem->ItemFlags[0] != 3)
		{
			if (TestBoundsCollide(doorItem, laraItem, coll->Setup.Radius))
			{
				if (HandleItemSphereCollision(*doorItem, *laraItem))
				{
					if (coll->Setup.EnableObjectPush)
						ItemPushItem(doorItem, laraItem, coll, 0, 1);
				}
			}
		}
	}
}
