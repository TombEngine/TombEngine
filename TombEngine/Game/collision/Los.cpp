#include "framework.h"
#include "Game/collision/Los.h"

#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/collision/sphere.h"
#include "Game/items.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Objects/game_object_ids.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Math;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

namespace TEN::Collision::Los
{
	static std::vector<ItemInfo*> GetNearbyItems(const std::vector<int>& roomNumbers)
	{
		// Collect neighbor room numbers.
		auto neighborRoomNumbers = std::set<int>{};
		for (int roomNumber : roomNumbers)
		{
			const auto& room = g_Level.Rooms[roomNumber];
			neighborRoomNumbers.insert(room.NeighborRoomNumbers.begin(), room.NeighborRoomNumbers.end());
		}

		// Run through neighbor rooms.
		auto items = std::vector<ItemInfo*>{};
		for (int neighborRoomNumber : neighborRoomNumbers)
		{
			const auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			if (!neighborRoom.Active())
				continue;

			// Run through items in room.
			int itemNumber = neighborRoom.itemNumber;
			while (itemNumber != NO_VALUE)
			{
				auto& item = g_Level.Items[itemNumber];

				// HACK: For some reason, infinite loop may sometimes occur.
				if (itemNumber == item.NextItem)
					break;
				itemNumber = item.NextItem;

				// 1) Ignore bridges (handled as part of room collision).
				if (item.IsBridge())
					continue;

				// 2) Check collidability.
				const auto& object = Objects[item.ObjectNumber];
				if (!item.Collidable || item.Flags & IFLAG_KILLED ||
					object.collision == nullptr || object.drawRoutine == nullptr)
				{
					continue;
				}

				// 3) Check status.
				if (item.Status == ItemStatus::ITEM_INVISIBLE || item.Status == ItemStatus::ITEM_DEACTIVATED)
					continue;

				items.push_back(&item);
			}
		}

		return items;
	}

	static std::vector<MESH_INFO*> GetNearbyStatics(const std::vector<int>& roomNumbers)
	{
		// Collect neighbor room numbers.
		auto neighborRoomNumbers = std::set<int>{};
		for (int roomNumber : roomNumbers)
		{
			const auto& room = g_Level.Rooms[roomNumber];
			neighborRoomNumbers.insert(room.NeighborRoomNumbers.begin(), room.NeighborRoomNumbers.end());
		}

		// Run through neighbor rooms.
		auto statics = std::vector<MESH_INFO*>{};
		for (int neighborRoomNumber : neighborRoomNumbers)
		{
			auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			if (!neighborRoom.Active())
				continue;

			// Run through statics.
			for (auto& staticObj : neighborRoom.mesh)
			{
				// Check visibility.
				if (!(staticObj.flags & StaticMeshFlags::SM_VISIBLE))
					continue;

				// Collect static.
				statics.push_back(&staticObj);
			}
		}

		return statics;
	}

	LosCollisionData GetLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist,
									 bool collideItems, bool collideSpheres, bool collideStatics)
	{
		// FAILSAFE.
		if (dir == Vector3::Zero)
		{
			TENLog("GetLosCollision(): dir is not a unit vector.", LogLevel::Warning);
			return {};
		}

		auto los = LosCollisionData{};

		// 1) Collect room LOS collision.
		los.Room = GetRoomLosCollision(origin, roomNumber, dir, dist);

		// 2) Collect item and sphere LOS collisions.
		if (collideItems || collideSpheres)
		{
			// Run through nearby items.
			auto items = GetNearbyItems(los.Room.RoomNumbers);
			for (auto* item : items)
			{
				// 2.1) Collect item LOS collisions.
				if (collideItems)
				{
					auto obb = item->GetObb();

					float intersectDist = 0.0f;
					if (obb.Intersects(origin, dir, intersectDist) && intersectDist <= los.Room.Distance)
					{
						auto pos = Geometry::TranslatePoint(origin, dir, intersectDist);
						auto offset = pos - item->Pose.Position.ToVector3();
						int roomNumber = GetPointCollision(item->Pose.Position, item->RoomNumber, offset).GetRoomNumber();

						auto itemLos = ItemLosCollisionData{};
						itemLos.Item = item;
						itemLos.Position = pos;
						itemLos.RoomNumber = roomNumber;
						itemLos.Distance = intersectDist;
						itemLos.IsOriginContained = (bool)obb.Contains(origin);
						los.Items.push_back(itemLos);
					}
				}

				// 2.2) Collect item sphere LOS collisions.
				if (collideSpheres)
				{
					auto spheres = item->GetSpheres();
					for (int i = 0; i < spheres.size(); i++)
					{
						const auto& sphere = spheres[i];

						float intersectDist = 0.0f;
						if (sphere.Intersects(origin, dir, intersectDist) && intersectDist <= los.Room.Distance)
						{
							auto pos = Geometry::TranslatePoint(origin, dir, intersectDist);
							auto offset = pos - item->Pose.Position.ToVector3();
							int roomNumber = GetPointCollision(item->Pose.Position, item->RoomNumber, offset).GetRoomNumber();

							auto sphereLos = SphereLosCollisionData{};
							sphereLos.Item = item;
							sphereLos.SphereID = i;
							sphereLos.Position = pos;
							sphereLos.RoomNumber = roomNumber;
							sphereLos.Distance = intersectDist;
							sphereLos.IsOriginContained = (bool)sphere.Contains(origin);
							los.Spheres.push_back(sphereLos);
						}
					}
				}
			}

			// 2.3) Sort item LOS collisions.
			std::sort(
				los.Items.begin(), los.Items.end(),
				[](const auto& itemLos0, const auto& itemLos1)
				{
					return (itemLos0.Distance < itemLos1.Distance);
				});

			// 2.4) Sort sphere LOS collisions.
			std::sort(
				los.Spheres.begin(), los.Spheres.end(),
				[](const auto& sphereLos0, const auto& sphereLos1)
				{
					return (sphereLos0.Distance < sphereLos1.Distance);
				});
		}

		// 3) Collect static LOS collisions.
		if (collideStatics)
		{
			// Run through nearby statics.
			auto statics = GetNearbyStatics(los.Room.RoomNumbers);
			for (auto* staticObj : statics)
			{
				auto obb = staticObj->GetObb();

				float intersectDist = 0.0f;
				if (obb.Intersects(origin, dir, intersectDist) && intersectDist <= los.Room.Distance)
				{
					auto pos = Geometry::TranslatePoint(origin, dir, intersectDist);
					auto offset = pos - staticObj->pos.Position.ToVector3();
					int roomNumber = GetPointCollision(staticObj->pos.Position, staticObj->roomNumber, offset).GetRoomNumber();

					auto staticLos = StaticLosCollisionData{};
					staticLos.Static = staticObj;
					staticLos.Position = pos;
					staticLos.RoomNumber = roomNumber;
					staticLos.Distance = intersectDist;
					staticLos.IsOriginContained = (bool)obb.Contains(origin);
					los.Statics.push_back(staticLos);
				}
			}

			// 3.1) Sort static LOS collisions.
			std::sort(
				los.Statics.begin(), los.Statics.end(),
				[](const auto& staticLos0, const auto& staticLos1)
				{
					return (staticLos0.Distance < staticLos1.Distance);
				});
		}

		// 4) Return sorted LOS collision.
		return los;
	}

	RoomLosCollisionData GetRoomLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collideBridges)
	{
		constexpr auto PORTAL_THRESHOLD = 0.001f;

		// FAILSAFE.
		if (dir == Vector3::Zero)
		{
			TENLog("GetRoomLosCollision(): Direction is not a unit vector.", LogLevel::Warning);
			return {};
		}

		auto roomLos = RoomLosCollisionData{};

		// 1) Define ray.
		auto ray = Ray(origin, dir);
		float rayDist = dist;
		int rayRoomNumber = roomNumber;

		// 2) Traverse rooms.
		bool traversePortal = true;
		while (traversePortal)
		{
			auto closestTri = std::optional<CollisionTriangleData>();
			float closestDist = rayDist;
			int portalRoomNumber = NO_VALUE;

			// 2.1) Clip room.
			const auto& room = g_Level.Rooms[rayRoomNumber];
			auto meshColl = room.CollisionMesh.GetCollision(ray, closestDist);
			if (meshColl.has_value())
			{
				closestTri = meshColl->Triangle;
				closestDist = meshColl->Distance;
			}

			// 2.2) Clip bridge (if applicable).
			if (collideBridges)
			{
				// Run through neighbor rooms.
				for (int neighborRoomNumber : room.NeighborRoomNumbers)
				{
					const auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];

					// Run through bounded bridges.
					auto bridgeItemNumbers = neighborRoom.Bridges.GetBoundedIds(ray, closestDist);
					for (int bridgeItemNumber : bridgeItemNumbers)
					{
						const auto& bridgeItem = g_Level.Items[bridgeItemNumber];
						const auto& bridge = GetBridgeObject(bridgeItem);

						// Check bridge status.
						if (bridgeItem.Status == ItemStatus::ITEM_DEACTIVATED || bridgeItem.Status == ItemStatus::ITEM_INVISIBLE)
							continue;

						// Clip bridge.
						auto meshColl = bridge.GetCollisionMesh().GetCollision(ray, closestDist);
						if (meshColl.has_value() && meshColl->Distance < closestDist)
						{
							closestTri = meshColl->Triangle;
							closestDist = meshColl->Distance;
						}
					}
				}
			}

			// 2.4) Clip portal.
			for (const auto& portal : room.Portals)
			{
				auto meshColl = portal.CollisionMesh.GetCollision(ray, closestDist);
				if (meshColl.has_value() && meshColl->Distance < closestDist &&
					abs(meshColl->Distance - closestDist) > PORTAL_THRESHOLD) // FAILSAFE: Prioritize tangible triangle in case portal triangle coincides.
				{
					closestTri = meshColl->Triangle;
					closestDist = meshColl->Distance;
					portalRoomNumber = portal.RoomNumber;
				}
			}

			// 2.5) Collect room number.
			roomLos.RoomNumbers.push_back(rayRoomNumber);

			// 2.6) Traverse portal or return room LOS.
			if (closestTri.has_value())
			{
				auto intersectPos = Geometry::TranslatePoint(ray.position, ray.direction, closestDist);

				// Hit portal triangle; update ray to traverse new room.
				if (portalRoomNumber != NO_VALUE &&
					rayRoomNumber != portalRoomNumber) // FAILSAFE: Prevent infinite loop if room portal leads back to itself.
				{
					auto prevIntersectPos = ray.position;

					ray.position = intersectPos;
					rayDist -= closestDist;
					rayRoomNumber = portalRoomNumber;

					// FAILSAFE: Prevent infinite loop if room portals lead back to each other.
					if (prevIntersectPos == intersectPos)
					{
						traversePortal = false;
						TENLog("GetRoomLosCollision(): Room portals cannot lead back to each other.", LogLevel::Warning);
					}
				}
				// Hit tangible triangle; collect remaining room LOS data.
				else
				{
					if (portalRoomNumber != NO_VALUE)
						TENLog("GetRoomLosCollision(): Room portal cannot lead back to itself.", LogLevel::Warning);

					roomLos.Triangle = *closestTri;
					roomLos.Position = intersectPos;
					roomLos.RoomNumber = rayRoomNumber;
					roomLos.IsIntersected = true;
					roomLos.Distance = Vector3::Distance(origin, intersectPos);

					traversePortal = false;
				}
			}
			else
			{
				roomLos.Triangle = std::nullopt;
				roomLos.Position = Geometry::TranslatePoint(ray.position, ray.direction, rayDist);
				roomLos.RoomNumber = rayRoomNumber;
				roomLos.IsIntersected = false;
				roomLos.Distance = dist;

				traversePortal = false;
			}
		}

		// 3) Return room LOS collision.
		return roomLos;
	}

	std::optional<ItemLosCollisionData> GetItemLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collidePlayer)
	{
		// Run through item LOS collisions.
		auto los = GetLosCollision(origin, roomNumber, dir, dist, true, false, false);
		for (auto& itemLos : los.Items)
		{
			// Check if item isn't player (if applicable).
			if (!collidePlayer && itemLos.Item->IsLara())
				continue;

			return itemLos;
		}

		return std::nullopt;
	}

	std::optional<SphereLosCollisionData> GetSphereLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collidePlayer)
	{
		// Run through sphere LOS collisions.
		auto los = GetLosCollision(origin, roomNumber, dir, dist, false, true, false);
		for (auto& sphereLos : los.Spheres)
		{
			// Check if item isn't player (if applicable).
			if (!collidePlayer && sphereLos.Item->IsLara())
				continue;

			return sphereLos;
		}

		return std::nullopt;
	}

	std::optional<StaticLosCollisionData> GetStaticLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collideOnlySolid)
	{
		// Run through static LOS collisions.
		auto los = GetLosCollision(origin, roomNumber, dir, dist, false, false, true);
		for (auto& staticLos : los.Statics)
		{
			// Check if static is solid (if applicable).
			if (collideOnlySolid && !(staticLos.Static->flags & StaticMeshFlags::SM_SOLID))
				continue;

			// Check if static is collidable.
			if (!(staticLos.Static->flags & StaticMeshFlags::SM_COLLISION))
				continue;

			return staticLos;
		}

		return std::nullopt;
	}

	std::pair<GameVector, GameVector> GetRayFrom2DPosition(const Vector2& screenPos)
	{
		auto posPair = g_Renderer.GetRay(screenPos);

		auto dir = posPair.second - posPair.first;
		dir.Normalize();
		float dist = Vector3::Distance(posPair.first, posPair.second);

		auto roomLos = GetRoomLosCollision(posPair.first, Camera.pos.RoomNumber, dir, dist);

		auto origin = GameVector(posPair.first, Camera.pos.RoomNumber);
		auto target = GameVector(roomLos.Position, roomLos.RoomNumber);
		return std::pair(origin, target);
	}
}
