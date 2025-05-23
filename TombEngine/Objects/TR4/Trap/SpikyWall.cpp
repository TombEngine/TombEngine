#include "framework.h"
#include "Objects/TR4/Trap/SpikyWall.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;
using namespace TEN::Math;

// NOTES:
// ItemFlags[0]: forward velocity.
// ItemFlags[1]: enable collision with other spiky walls.

namespace TEN::Entities::Traps
{
	constexpr auto SPIKY_WALL_HARM_DAMAGE = 15;

	void InitializeSpikyWall(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[0] = item.TriggerFlags;
		item.ItemFlags[1] = 0;
	}

	void ControlSpikyWall(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		item.ItemFlags[0] = item.TriggerFlags;
		
		if (item.ItemFlags[1] && item.ItemFlags[0])
			item.ItemFlags[1] -= 1;

		int forwardVel = item.ItemFlags[0];
		auto bounds = GameBoundingBox(&item);

		auto pointColl0 = GetPointCollision(item, item.Pose.Orientation.y, (forwardVel >= 0) ? bounds.Z2 : bounds.Z1, bounds.Y2);
		auto pointColl1 = GetPointCollision(item, item.Pose.Orientation.y, (forwardVel >= 0) ? bounds.Z2 : bounds.Z1, bounds.Y2, (bounds.X2 - bounds.X1) / 2);

		auto collObjects = GetCollidedObjects(item, true, true);
		if (!collObjects.IsEmpty())
		{
			for (auto* itemPtr : collObjects.Items)
			{
				const auto& object = Objects[itemPtr->ObjectNumber];

				if (object.intelligent)
				{
					itemPtr->HitPoints = 0;
				}
				else if (itemPtr->ObjectNumber == ID_SPIKY_WALL && !item.ItemFlags[1])
				{
					itemPtr->TriggerFlags = 0;

					item.TriggerFlags = 0;
					item.Status = ITEM_DEACTIVATED;
					item.ItemFlags[1] = 10;

					StopSoundEffect(SFX_TR4_ROLLING_BALL);
				}
			}
		}

		// Stop moving.
		if (!item.TriggerFlags ||
			pointColl0.GetSector().IsWall(item.Pose.Position.x, item.Pose.Position.z) || 
			pointColl0.GetSector().Stopper ||
			pointColl1.GetSector().IsWall(item.Pose.Position.x, item.Pose.Position.z) ||
			pointColl1.GetSector().Stopper)
		{
			auto& room = g_Level.Rooms[item.RoomNumber];
			for (auto& mesh : room.mesh)
			{
				if ((abs(pointColl0.GetPosition().x - mesh.pos.Position.x) < BLOCK(1) &&
					abs(pointColl0.GetPosition().z - mesh.pos.Position.z) < BLOCK(1)) ||
					abs(pointColl1.GetPosition().x - mesh.pos.Position.x) < BLOCK(1) &&
					abs(pointColl1.GetPosition().z - mesh.pos.Position.z) < BLOCK(1) &&
					Statics[mesh.staticNumber].shatterType != ShatterType::None)
				{					
					if (mesh.HitPoints != 0)
						continue;

					mesh.HitPoints -= 1;
					ShatterObject(nullptr, &mesh, -64, LaraItem->RoomNumber, 0);
					SoundEffect(SFX_TR4_SMASH_ROCK, &item.Pose);
					TestTriggers(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, item.RoomNumber, true);
				}
			}

			item.Status = ITEM_DEACTIVATED;
			StopSoundEffect(SFX_TR4_ROLLING_BALL);
		}
		// Move.
		else
		{
			item.Pose.Position = Geometry::TranslatePoint(item.Pose.Position, item.Pose.Orientation.y, forwardVel);
			item.Status = ITEM_ACTIVE;

			if (pointColl0.GetRoomNumber() != item.RoomNumber)
				ItemNewRoom(itemNumber, pointColl0.GetRoomNumber());

			SoundEffect(SFX_TR4_ROLLING_BALL, &item.Pose);
		}
	}

	void CollideSpikyWall(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.ObjectNumber != ID_SPIKY_WALL)
			return;

		// Collide with objects.
		if (item.Status == ITEM_ACTIVE)
		{
			if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
				return;

			HandleItemSphereCollision(item, *playerItem);
		}
		else if (item.Status != ITEM_INVISIBLE)
		{
			ObjectCollision(itemNumber, playerItem, coll);
		}

		// Damage entity.
		if (TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
		{
			DoDamage(playerItem, SPIKY_WALL_HARM_DAMAGE);
			DoLotsOfBlood(playerItem->Pose.Position.x, playerItem->Pose.Position.y + CLICK(2), playerItem->Pose.Position.z, 4, playerItem->Pose.Orientation.y, playerItem->RoomNumber, 3);
			playerItem->TouchBits.ClearAll();

			SoundEffect(SFX_TR4_LARA_GRABFEET, &playerItem->Pose);

			// Push player.
			float vel = playerItem->Animation.Velocity.z;
			playerItem->Pose.Position = Geometry::TranslatePoint(
				playerItem->Pose.Position, item.Pose.Orientation.y,
				(item.ItemFlags[0] > 0) ? (item.ItemFlags[0] + vel) : (item.ItemFlags[0] - vel));
		}
	}
}
