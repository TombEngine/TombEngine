#include "framework.h"
#include "Objects/TR3/Trap/HeavyStamper.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;

namespace TEN::Entities::Traps
{	
	constexpr auto HEAVY_STAMPER_DAMAGE = 70;
	constexpr auto HEAVY_STAMPER_IMPACT_FRAME = 40;
	constexpr auto PLAYER_HP_DEATH_THRESHOLD = 400;

	void ControlHeavyStamper(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		if (item.Animation.FrameNumber >= HEAVY_STAMPER_IMPACT_FRAME)
		{
			Camera.bounce = -9;
			item.ItemFlags[4] = HEAVY_STAMPER_DAMAGE;  // NOTE: Avoid the stamper pushing the player in GenericSphereBoxCollision().
		}
		else
		{
			item.ItemFlags[4] = 0;
		}		
			
		item.ItemFlags[0] = 1;
		item.ItemFlags[3] = HEAVY_STAMPER_DAMAGE;
			
		AnimateItem(&item);
	}
		
	void CollideHeavyStamper(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];
		if (item.Status == ITEM_INVISIBLE)
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

		int harmBits = item.ItemFlags[0]; // NOTE: Value spread across ItemFlags[0] and ItemFlags[1].

		auto collidedBits = item.TouchBits;
		if (item.ItemFlags[2] != 0)
			collidedBits.Clear(0);

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

				if ((ItemPushItem(&item, playerItem, coll, harmBits & 1, 0) && (harmBits & 1) && (item.ItemFlags[3] > 0)) || item.ItemFlags[4] == HEAVY_STAMPER_DAMAGE)
				{
					if (playerItem->HitPoints < PLAYER_HP_DEATH_THRESHOLD && item.Animation.FrameNumber >= HEAVY_STAMPER_IMPACT_FRAME)
					{
						if (item.TriggerFlags < 0)
							playerItem->Pose.Scale = Vector3(1.0f, 0.1f, 1.0f);

						auto pointColl = GetPointCollision(*playerItem);

						playerItem->Pose.Position.y = pointColl.GetFloorHeight();
						playerItem->Animation.Velocity = Vector3::Zero;
						playerItem->Animation.IsAirborne = false;

						DoDamage(playerItem, INT_MAX);
						SetAnimation(playerItem, LA_BOULDER_DEATH);
						return;
					}

					DoDamage(playerItem, HEAVY_STAMPER_DAMAGE + item.ItemFlags[4]); // NOTE: Doubles damage when the player isn’t pushed, maintaining consistent damage at the object’s edges.
					TriggerLaraBlood();
				}
			}
		}
	}
}
