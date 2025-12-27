#include "framework.h"
#include "Objects/TR3/Trap/Fan.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Collision::Point;

namespace TEN::Entities::Traps
{
	constexpr auto FAN_HARM_DAMAGE = 5000;
	constexpr auto FAN_HARM_MESH = 0x2;

	enum FanState
	{
		FAN_STATE_ROTATING = 0,
		FAN_STATE_IDLE = 1,
	};

	enum FanAnim
	{
		FAN_ANIM_ROTATING = 0,
		FAN_ANIM_STOPPING = 1,
		FAN_ANIM_IDLE = 2,
		FAN_ANIM_STARTING = 3,
	};

	void InitializeFan(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.ItemFlags[0] = FAN_HARM_MESH;
	}

	void ControlFan(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			if (item.Animation.TargetState != FAN_STATE_ROTATING)
			{
				item.Animation.TargetState = FAN_STATE_ROTATING;
				item.Status = ITEM_ACTIVE;
			}

			item.ItemFlags[0] = FAN_HARM_MESH;
		}
		else
		{
			if (item.Animation.TargetState != FAN_STATE_IDLE)
			{
				item.Animation.TargetState = FAN_STATE_IDLE;
			}
			else
			{
				if (item.Animation.AnimNumber == FAN_ANIM_IDLE &&
					item.Animation.FrameNumber == GetAnimData(item).EndFrameNumber)
				{
					item.Status = ITEM_NOT_ACTIVE;
				}
			}
		}

		item.ItemFlags[3] = item.TriggerFlags;

		AnimateItem(&item);
	}

	void CollideFan(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];
		if (item.Status == ITEM_INVISIBLE || item.Status == ITEM_NOT_ACTIVE)
			return;

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;

		HandleItemSphereCollision(item, *playerItem);
		if (!item.TouchBits.TestAny())
			return;

		short prevYOrient = item.Pose.Orientation.y;
		item.Pose.Orientation.y = 0;
		auto spheres = item.GetSpheres();
		item.Pose.Orientation.y = prevYOrient;

		int harmBits = *(int*)&item.ItemFlags[0]; // NOTE: Value spread across ItemFlags[0] and ItemFlags[1].

		auto collidedBits = item.TouchBits;

		coll->Setup.EnableObjectPush = (item.ItemFlags[4] == 0);

		// Handle push and damage.
		for (int i = 0; i < spheres.size(); i++)
		{
			if (collidedBits.Test(i))
			{
				const auto& sphere = spheres[i];

				GlobalCollisionBounds.X1 = sphere.Center.x - sphere.Radius - item.Pose.Position.x;
				GlobalCollisionBounds.X2 = sphere.Center.x + sphere.Radius - item.Pose.Position.x;
				GlobalCollisionBounds.Y1 = sphere.Center.y - sphere.Radius - item.Pose.Position.y;
				GlobalCollisionBounds.Y2 = sphere.Center.y + sphere.Radius - item.Pose.Position.y;
				GlobalCollisionBounds.Z1 = sphere.Center.z - sphere.Radius - item.Pose.Position.z;
				GlobalCollisionBounds.Z2 = sphere.Center.z + sphere.Radius - item.Pose.Position.z;

				if ( (harmBits & 1) && (item.ItemFlags[3] > 0))
				{
					DoDamage(playerItem, item.ItemFlags[3]);
					TriggerLaraBlood();
					DoLotsOfBlood(LaraItem->Pose.Position.x, LaraItem->Pose.Position.y - CLICK(2), LaraItem->Pose.Position.z, (short)(item.Animation.Velocity.z * 2), LaraItem->Pose.Orientation.y, LaraItem->RoomNumber, 2);					
				}
			}

			harmBits >>= 1;
		}
	}
}
