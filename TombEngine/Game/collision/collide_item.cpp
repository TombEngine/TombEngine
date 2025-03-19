#include "framework.h"
#include "Game/collision/collide_item.h"

#include "Game/Animation/Animation.h"
#include "Game/control/los.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/simple_particle.h"
#include "Game/effects/Splash.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/pickup/pickup.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Sound/sound.h"
#include "Specific/winmain.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Splash;
using namespace TEN::Math;

constexpr auto ANIMATED_ALIGNMENT_FRAME_COUNT_THRESHOLD = 6;
constexpr auto COLLIDABLE_BOUNDS_THRESHOLD = 4;

// Globals

GameBoundingBox GlobalCollisionBounds;

void GenericSphereBoxCollision(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
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

	int harmBits = *(int*)&item.ItemFlags[0]; // NOTE: Value spread across ItemFlags[0] and ItemFlags[1].
	
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

			auto pos = playerItem->Pose.Position;
			if (ItemPushItem(&item, playerItem, coll, harmBits & 1, 3) && (harmBits & 1))
			{
				DoDamage(playerItem, item.ItemFlags[3]);

				auto deltaPos = pos - playerItem->Pose.Position;
				if (deltaPos != Vector3i::Zero)
				{
					if (TriggerActive(&item))
						TriggerLaraBlood();
				}

				if (!coll->Setup.EnableObjectPush)
					playerItem->Pose.Position += deltaPos;
			}
		}

		harmBits >>= 1;
	}
}

CollidedObjectData GetCollidedObjects(ItemInfo& collidingItem, bool onlyVisible, bool ignorePlayer, float customRadius, ObjectCollectionMode mode)
{
	constexpr auto ROUGH_BOX_HEIGHT_MIN = BLOCK(1 / 8.0f);

	auto collObjects = CollidedObjectData{};

	int itemCount	= 0;
	int staticCount = 0;

	// Establish parameters of colliding item.
	const auto& collidingBounds = GetClosestKeyframe(collidingItem).BoundingBox;

	// Quickly discard collision if colliding item bounds are below tolerance threshold.
	if (!customRadius && collidingBounds.GetExtents().Length() <= COLLIDABLE_BOUNDS_THRESHOLD)
		return collObjects;

	// Convert bounding box to DX bounds.
	auto convertedBounds = collidingBounds.ToBoundingOrientedBox(collidingItem.Pose);

	// Create conservative AABB for rough tests.
	auto collidingAabb = collidingBounds.ToConservativeBoundingBox(collidingItem.Pose);

	// Override extents if specified.
	if (customRadius > 0.0f)
	{
		collidingAabb = BoundingBox(collidingItem.Pose.Position.ToVector3(), Vector3(customRadius));
		convertedBounds.Extents = Vector3(customRadius);
	}

	// Run through neighboring rooms.
	const auto& room = g_Level.Rooms[collidingItem.RoomNumber];
	for (int roomNumber : room.NeighborRoomNumbers)
	{
		auto& neighborRoom = g_Level.Rooms[roomNumber];
		if (!neighborRoom.Active())
			continue;

		// Collect items.
		if (mode == ObjectCollectionMode::All ||
			mode == ObjectCollectionMode::Items)
		{
			int itemNumber = neighborRoom.itemNumber;
			if (itemNumber != NO_VALUE)
			{
				do
				{
					auto& item = g_Level.Items[itemNumber];
					const auto& object = Objects[item.ObjectNumber];

					itemNumber = item.NextItem;

					// Ignore player (if applicable).
					if (ignorePlayer && item.IsLara())
						continue;

					// Ignore invisible item (if applicable).
					if (onlyVisible && item.Status == ITEM_INVISIBLE)
						continue;

					// Ignore items not feasible for collision.
					if (item.Index == collidingItem.Index || item.Flags & IFLAG_KILLED || item.MeshBits == NO_JOINT_BITS)
						continue;

					// Ignore non-collidable non-player.
					if (!item.IsLara() && (!item.Collidable || object.drawRoutine == nullptr || object.collision == nullptr))
						continue;

					// HACK: Ignore UPV and big gun.
					if ((item.ObjectNumber == ID_UPV || item.ObjectNumber == ID_BIGGUN) && item.HitPoints == 1)
						continue;

					// Test rough distance to discard objects more than 6 blocks away.
					float dist = Vector3i::Distance(item.Pose.Position, collidingItem.Pose.Position);
					if (dist > COLLISION_CHECK_DISTANCE)
						continue;

					// If item bounding box extents is below tolerance threshold, discard object.
					const auto& bounds = GetClosestKeyframe(item).BoundingBox;
					if (bounds.GetExtents().Length() <= COLLIDABLE_BOUNDS_THRESHOLD)
						continue;

					// Test conservative AABB intersection.
					auto aabb = bounds.ToConservativeBoundingBox(item.Pose);
					if (!aabb.Intersects(collidingAabb))
						continue;

					// Test accurate OBB intersection.
					auto obb = bounds.ToBoundingOrientedBox(item.Pose);
					if (obb.Intersects(convertedBounds))
						collObjects.Items.push_back(&item);
				}
				while (itemNumber != NO_VALUE);
			}
		}

		// Collect statics.
		if (mode == ObjectCollectionMode::All ||
			mode == ObjectCollectionMode::Statics)
		{
			for (auto& staticObj : neighborRoom.mesh)
			{
				// Discard invisible statics.
				if (!(staticObj.flags & StaticMeshFlags::SM_VISIBLE))
					continue;

				// Test rough distance to discard statics beyond collision check threshold.
				float dist = Vector3i::Distance(staticObj.pos.Position, collidingItem.Pose.Position);
				if (dist > COLLISION_CHECK_DISTANCE)
					continue;

				// Skip if either bounding box has any zero extent (not a collidable volume).
				const auto& bounds = GetBoundsAccurate(staticObj, false);
				if (bounds.GetExtents().Length() <= COLLIDABLE_BOUNDS_THRESHOLD)
					continue;

				// Test conservative AABB intersection.
				auto aabb = bounds.ToConservativeBoundingBox(staticObj.pos);
				if (!aabb.Intersects(collidingAabb))
					continue;

				// Test accurate OBB intersection.
				auto obb = bounds.ToBoundingOrientedBox(staticObj.pos.Position);
				if (obb.Intersects(convertedBounds))
					collObjects.Statics.push_back(&staticObj);
			}
		}
	}

	return collObjects;
}

bool TestWithGlobalCollisionBounds(ItemInfo* item, ItemInfo* laraItem, CollisionInfo* coll)
{
	const auto& bounds = GetClosestKeyframe(*laraItem).BoundingBox;

	if ((item->Pose.Position.y + GlobalCollisionBounds.Y2) <= (laraItem->Pose.Position.y + bounds.Y1))
		return false;

	if ((item->Pose.Position.y + GlobalCollisionBounds.Y1) >= bounds.Y2)
		return false;

	float sinY = phd_sin(item->Pose.Orientation.y);
	float cosY = phd_cos(item->Pose.Orientation.y);

	int dx = laraItem->Pose.Position.x - item->Pose.Position.x;
	int dz = laraItem->Pose.Position.z - item->Pose.Position.z;

	int x = (dx * cosY) - (dz * sinY);
	int z = (dz * cosY) + (dx * sinY);

	if (x < (GlobalCollisionBounds.X1 - coll->Setup.Radius) ||
		x > (GlobalCollisionBounds.X2 + coll->Setup.Radius) ||
		z < (GlobalCollisionBounds.Z1 - coll->Setup.Radius) ||
		z > (GlobalCollisionBounds.Z2 + coll->Setup.Radius))
	{
		return false;
	}

	return true;
}

void TestForObjectOnLedge(ItemInfo* item, CollisionInfo* coll)
{
	auto bbox = GameBoundingBox(item).ToBoundingOrientedBox(item->Pose);
	auto height = (bbox.Center - bbox.Extents).y - CLICK(1);

	for (int i = 0; i < 3; i++)
	{
		auto sinHeading = (i != 1) ? (phd_sin(coll->Setup.ForwardAngle + ANGLE((i * 90.0f) - 90.0f))) : 0;
		auto cosHeading = (i != 1) ? (phd_cos(coll->Setup.ForwardAngle + ANGLE((i * 90.0f) - 90.0f))) : 0;

		auto origin = Vector3(
			item->Pose.Position.x + (sinHeading * (coll->Setup.Radius)),
			height,
			item->Pose.Position.z + (cosHeading * (coll->Setup.Radius)));

		auto mxR = Matrix::CreateFromYawPitchRoll(TO_RAD(coll->Setup.ForwardAngle), 0.0f, 0.0f);
		auto direction = (Matrix::CreateTranslation(Vector3::UnitZ) * mxR).Translation();

		// DrawDebugSphere(origin, 16, Vector4::One, RendererDebugPage::CollisionStats);

		for (auto i : g_Level.Rooms[item->RoomNumber].NeighborRoomNumbers)
		{
			if (!g_Level.Rooms[i].Active())
				continue;

			int itemNumber = g_Level.Rooms[i].itemNumber;
			while (itemNumber != NO_VALUE)
			{
				auto* item2 = &g_Level.Items[itemNumber];
				auto* object = &Objects[item2->ObjectNumber];

				if (object->isPickup || object->collision == nullptr || !item2->Collidable || item2->Status == ITEM_INVISIBLE)
				{
					itemNumber = item2->NextItem;
					continue;
				}

				if (Vector3i::Distance(item->Pose.Position, item2->Pose.Position) < COLLISION_CHECK_DISTANCE)
				{
					auto box = GameBoundingBox(item2).ToBoundingOrientedBox(item2->Pose);
					float distance;

					if (box.Intersects(origin, direction, distance) && distance < (coll->Setup.Radius * 2))
					{
						coll->HitStatic = true;
						return;
					}
				}

				itemNumber = item2->NextItem;
			}

			for (auto& mesh : g_Level.Rooms[i].mesh)
			{
				if (!(mesh.flags & StaticMeshFlags::SM_VISIBLE))
					continue;

				if (Vector3i::Distance(item->Pose.Position, mesh.pos.Position) < COLLISION_CHECK_DISTANCE)
				{
					const auto& bBox = GetBoundsAccurate(mesh, false).ToBoundingOrientedBox(mesh.pos);
					float distance;

					if (bBox.Intersects(origin, direction, distance) && distance < (coll->Setup.Radius * 2))
					{
						coll->HitStatic = true;
						return;
					}
				}
			}
		}
	}
}

bool TestLaraPosition(const ObjectCollisionBounds& bounds, ItemInfo* item, ItemInfo* laraItem)
{
	constexpr auto DEBUG_BOX_COLOR = Color(1.0f, 0.0f, 0.0f);

	// Draw oriented debug interaction box.
	if (DebugMode)
	{
		auto obb = bounds.BoundingBox.ToBoundingOrientedBox(item->Pose);
		auto rotMatrix = item->Pose.Orientation.ToRotationMatrix();

		DrawDebugBox(obb, DEBUG_BOX_COLOR, RendererDebugPage::CollisionStats);
		DrawDebugLine(
			obb.Center + Vector3::Transform(Vector3(0.0f, -obb.Extents.y, 0.0f), rotMatrix),
			obb.Center + Vector3::Transform(Vector3(0.0f, -obb.Extents.y, obb.Extents.z), rotMatrix),
			DEBUG_BOX_COLOR, RendererDebugPage::CollisionStats);
		DrawDebugLine(
			obb.Center + Vector3::Transform(Vector3(0.0f, -obb.Extents.y, obb.Extents.z), rotMatrix),
			obb.Center + Vector3::Transform(Vector3(0.0f, obb.Extents.y, obb.Extents.z), rotMatrix),
			DEBUG_BOX_COLOR, RendererDebugPage::CollisionStats);
		DrawDebugLine(
			obb.Center + Vector3::Transform(Vector3(0.0f, obb.Extents.y, 0.0f), rotMatrix),
			obb.Center + Vector3::Transform(Vector3(0.0f, obb.Extents.y, obb.Extents.z), rotMatrix),
			DEBUG_BOX_COLOR, RendererDebugPage::CollisionStats);
	}

	auto deltaOrient = laraItem->Pose.Orientation - item->Pose.Orientation;
	if (deltaOrient.x < bounds.OrientConstraint.first.x || deltaOrient.x > bounds.OrientConstraint.second.x ||
		deltaOrient.y < bounds.OrientConstraint.first.y || deltaOrient.y > bounds.OrientConstraint.second.y ||
		deltaOrient.z < bounds.OrientConstraint.first.z || deltaOrient.z > bounds.OrientConstraint.second.z)
	{
		return false;
	}

	auto pos = (laraItem->Pose.Position - item->Pose.Position).ToVector3();
	auto rotMatrix = item->Pose.Orientation.ToRotationMatrix();

	// NOTE: Transpose = faster inverse.
	rotMatrix = rotMatrix.Transpose();

	pos = Vector3::Transform(pos, rotMatrix);

	if (pos.x < bounds.BoundingBox.X1 || pos.x > bounds.BoundingBox.X2 ||
		pos.y < bounds.BoundingBox.Y1 || pos.y > bounds.BoundingBox.Y2 ||
		pos.z < bounds.BoundingBox.Z1 || pos.z > bounds.BoundingBox.Z2)
	{
		return false;
	}

	return true;
}

bool AlignLaraPosition(const Vector3i& offset, ItemInfo* item, ItemInfo* laraItem)
{
	auto* lara = GetLaraInfo(laraItem);

	laraItem->Pose.Orientation = item->Pose.Orientation;

	auto rotMatrix = item->Pose.Orientation.ToRotationMatrix();
	auto pos = Vector3::Transform(offset.ToVector3(), rotMatrix);
	auto target = item->Pose.Position.ToVector3() + pos;

	int height = GetPointCollision(target, laraItem->RoomNumber).GetFloorHeight();
	if ((laraItem->Pose.Position.y - height) <= CLICK(2))
	{
		laraItem->Pose.Position = Vector3i(target);
		return true;
	}

	if (lara->Control.IsMoving)
	{
		lara->Control.IsMoving = false;
		lara->Control.HandStatus = HandStatus::Free;
	}

	return false;
}

bool MoveLaraPosition(const Vector3i& offset, ItemInfo* item, ItemInfo* laraItem)
{
	auto* lara = GetLaraInfo(laraItem);

	auto rotMatrix = item->Pose.Orientation.ToRotationMatrix();
	auto pos = Vector3::Transform(offset.ToVector3(), rotMatrix);
	auto target = Pose(item->Pose.Position + Vector3i(pos), item->Pose.Orientation);

	if (!Objects[item->ObjectNumber].isPickup)
		return Move3DPosTo3DPos(laraItem, laraItem->Pose, target, LARA_ALIGN_VELOCITY, ANGLE(2.0f));
	else
	{
		// Prevent picking up items which can result in so called "flare pickup bug"
		int height = GetPointCollision(target.Position, laraItem->RoomNumber).GetFloorHeight();
		if (abs(height - laraItem->Pose.Position.y) <= CLICK(2))
			return Move3DPosTo3DPos(laraItem, laraItem->Pose, target, LARA_ALIGN_VELOCITY, ANGLE(2.0f));
	}

	if (lara->Control.IsMoving)
	{
		lara->Control.IsMoving = false;
		lara->Control.HandStatus = HandStatus::Free;
	}

	return false;
}

static bool ItemCollide(int value, int radius)
{
	return ((value >= -radius) && (value <= radius));
}

static bool ItemInRange(int x, int z, int radius)
{
	return ((SQUARE(x) + SQUARE(z)) <= SQUARE(radius));
}

bool ItemNearLara(const Vector3i& origin, int radius)
{
	auto target = GameVector(origin - LaraItem->Pose.Position);

	if (!ItemCollide(target.y, ITEM_RADIUS_YMAX))
		return false;

	if (!ItemCollide(target.x, radius) || !ItemCollide(target.z, radius))
		return false;

	if (!ItemInRange(target.x, target.z, radius))
		return false;

	auto bounds = GameBoundingBox(LaraItem);
	if (target.y >= bounds.Y1 && target.y <= (bounds.Y2 + LARA_RADIUS))
		return true;

	return false;
}

bool ItemNearTarget(const Vector3i& origin, ItemInfo* targetEntity, int radius)
{
	auto pos = origin - targetEntity->Pose.Position;

	if (!ItemCollide(pos.y, ITEM_RADIUS_YMAX))
		return false;

	if (!ItemCollide(pos.x, radius) || !ItemCollide(pos.z, radius))
		return false;

	if (!ItemInRange(pos.x, pos.z, radius))
		return false;

	auto bounds = GameBoundingBox(targetEntity);
	if (pos.y >= bounds.Y1 && pos.y <= bounds.Y2)
		return true;

	return false;
}

bool Move3DPosTo3DPos(ItemInfo* item, Pose& fromPose, const Pose& toPose, int velocity, short turnRate)
{
	auto* lara = GetLaraInfo(item);

	auto direction = toPose.Position - fromPose.Position;
	float distance = Vector3i::Distance(fromPose.Position, toPose.Position);

	if (velocity < distance)
		fromPose.Position += direction * (velocity / distance);
	else
		fromPose.Position = toPose.Position;

	if (!lara->Control.IsMoving)
	{
		bool shouldAnimate = ((distance - velocity) > (velocity * ANIMATED_ALIGNMENT_FRAME_COUNT_THRESHOLD));

		if (shouldAnimate && lara->Control.WaterStatus != WaterStatus::Underwater)
		{
			short angle = Geometry::GetOrientToPoint(fromPose.Position.ToVector3(), toPose.Position.ToVector3()).y;
			int direction = (GetQuadrant(angle) - GetQuadrant(fromPose.Orientation.y)) & 3;

			switch (direction)
			{
			default:
			case NORTH:
				SetAnimation(*item, LA_WALK);
				break;

			case SOUTH:
				SetAnimation(*item, LA_WALK_BACK);
				break;

			case EAST:
				SetAnimation(*item, LA_SIDESTEP_RIGHT);
				break;

			case WEST:
				SetAnimation(*item, LA_SIDESTEP_LEFT);
				break;
			}

			lara->Control.HandStatus = HandStatus::Busy;
		}

		lara->Control.IsMoving = true;
		lara->Control.Count.PositionAdjust = 0;
	}

	auto deltaOrient = toPose.Orientation - fromPose.Orientation;

	if (deltaOrient.x > turnRate)
		fromPose.Orientation.x += turnRate;
	else if (deltaOrient.x < -turnRate)
		fromPose.Orientation.x -= turnRate;
	else
		fromPose.Orientation.x = toPose.Orientation.x;

	if (deltaOrient.y > turnRate)
		fromPose.Orientation.y += turnRate;
	else if (deltaOrient.y < -turnRate)
		fromPose.Orientation.y -= turnRate;
	else
		fromPose.Orientation.y = toPose.Orientation.y;

	if (deltaOrient.z > turnRate)
		fromPose.Orientation.z += turnRate;
	else if (deltaOrient.z < -turnRate)
		fromPose.Orientation.z -= turnRate;
	else
		fromPose.Orientation.z = toPose.Orientation.z;

	return (fromPose == toPose);
}

bool TestBoundsCollide(ItemInfo* item, ItemInfo* laraItem, int radius)
{
	const auto& bounds = GetClosestKeyframe(*item).BoundingBox;
	const auto& playerBounds = GetClosestKeyframe(*laraItem).BoundingBox;

	if (bounds.GetExtents() == Vector3::Zero || playerBounds.GetExtents() == Vector3::Zero)
		return false;

	if ((item->Pose.Position.y + bounds.Y2) <= (laraItem->Pose.Position.y + playerBounds.Y1))
		return false;

	if ((item->Pose.Position.y + bounds.Y1) >= (laraItem->Pose.Position.y + playerBounds.Y2))
		return false;

	float sinY = phd_sin(item->Pose.Orientation.y);
	float cosY = phd_cos(item->Pose.Orientation.y);

	int x = laraItem->Pose.Position.x - item->Pose.Position.x;
	int z = laraItem->Pose.Position.z - item->Pose.Position.z;
	int dx = (x * cosY) - (z * sinY);
	int dz = (z * cosY) + (x * sinY);

	if (dx >= (bounds.X1 - radius) &&
		dx <= (bounds.X2 + radius) &&
		dz >= (bounds.Z1 - radius) &&
		dz <= (bounds.Z2 + radius))
	{
		return true;
	}

	return false;
}

bool TestBoundsCollideStatic(ItemInfo* item, const MESH_INFO& mesh, int radius)
{
	const auto& bounds = GetBoundsAccurate(mesh, false);

	if (!(bounds.Z2 != 0 || bounds.Z1 != 0 || bounds.X1 != 0 || bounds.X2 != 0 || bounds.Y1 != 0 || bounds.Y2 != 0))
		return false;

	const auto& itemBounds = GetClosestKeyframe(*item).BoundingBox;
	if (mesh.pos.Position.y + bounds.Y2 <= item->Pose.Position.y + itemBounds.Y1)
		return false;

	if (mesh.pos.Position.y + bounds.Y1 >= item->Pose.Position.y + itemBounds.Y2)
		return false;

	float sinY = phd_sin(mesh.pos.Orientation.y);
	float cosY = phd_cos(mesh.pos.Orientation.y);

	int x = item->Pose.Position.x - mesh.pos.Position.x;
	int z = item->Pose.Position.z - mesh.pos.Position.z;
	int dx = (x * cosY) - (z * sinY);
	int dz = (z * cosY) + (x * sinY);

	if (dx <= (radius + bounds.X2) &&
		dx >= (bounds.X1 - radius) &&
		dz <= (radius + bounds.Z2) &&
		dz >= (bounds.Z1 - radius))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool ItemPushItem(ItemInfo* item0, ItemInfo* item1, CollisionInfo* coll, bool enableSpasm, char bigPushFlags)
{
	constexpr auto SOFT_PUSH_LERP_ALPHA = 0.25f;

	const auto& object = Objects[item0->ObjectNumber];

	auto deltaPos = item1->Pose.Position - item0->Pose.Position;

	// Rotate relative position into item frame.
	auto rotMatrix = Matrix::CreateRotationY(TO_RAD(-item0->Pose.Orientation.y));
	auto relDeltaPos = Vector3i(Vector3::Transform(deltaPos.ToVector3(), rotMatrix));

	const auto& bounds = (bigPushFlags & 2) ? GlobalCollisionBounds : GameBoundingBox(item0);
	int minX = bounds.X1;
	int maxX = bounds.X2;
	int minZ = bounds.Z1;
	int maxZ = bounds.Z2;

	if (bigPushFlags & 1)
	{
		minX -= coll->Setup.Radius;
		maxX += coll->Setup.Radius;
		minZ -= coll->Setup.Radius;
		maxZ += coll->Setup.Radius;
	}

	// Big enemies.
	if (abs(deltaPos.x) > BLOCK(4.5f) || abs(deltaPos.z) > BLOCK(4.5f) ||
		relDeltaPos.x <= minX || relDeltaPos.x >= maxX ||
		relDeltaPos.z <= minZ || relDeltaPos.z >= maxZ)
	{
		return false;
	}

	int front = maxZ - relDeltaPos.z;
	int back = relDeltaPos.z - minZ;
	int left = relDeltaPos.x - minX;
	int right = maxX - relDeltaPos.x;

	// Account for collision radius.
	if (right <= left && right <= front && right <= back)
	{
		relDeltaPos.x += right;
	}
	else if (left <= right && left <= front && left <= back)
	{
		relDeltaPos.x -= left;
	}
	else if (front <= left && front <= right && front <= back)
	{
		relDeltaPos.z += front;
	}
	else
	{
		relDeltaPos.z -= back;
	}

	// Lerp to new position.
	auto newDeltaPos = Vector3i(Vector3::Transform(relDeltaPos.ToVector3(), rotMatrix.Invert()));
	if (object.intelligent)
	{
		// TODO: Improve sphere-dependent pushes.
		// Current sphere-box push combination is extremely unstable with large creatures such as the TR2 dragon. -- Sezz 2023.11.21
		// Possible solution: lerp to the position of a sphere's tangent toward the nearest box edge.
		// Must somehow determine the best sphere to reference. Maybe the one which pushes farthest?
		// Also, clamp distance the box edge to avoid overshooting.

		item1->Pose.Position.Lerp(item0->Pose.Position + newDeltaPos, SOFT_PUSH_LERP_ALPHA);
	}
	// Snap to new position.
	else if (coll->Setup.EnableObjectPush)
	{
		item1->Pose.Position = item0->Pose.Position + newDeltaPos;
	}

	if (item1->IsLara() && enableSpasm && bounds.GetHeight() > CLICK(1))
	{
		auto& player = GetLaraInfo(*item1);

		// TODO: Rewrite player spasm effect.
		player.HitDirection = NORTH;
		player.HitFrame = 0;
		
		// Dummy hurt call for sound.
		DoDamage(item1, 0); 
	}

	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.UpperCeilingBound = MAX_HEIGHT;

	short headingAngle = coll->Setup.ForwardAngle;
	coll->Setup.ForwardAngle = phd_atan(item1->Pose.Position.z - coll->Setup.PrevPosition.z, item1->Pose.Position.x - coll->Setup.PrevPosition.x);
	GetCollisionInfo(coll, item1);
	coll->Setup.ForwardAngle = headingAngle;

	if (coll->CollisionType == CollisionType::None)
	{
		coll->Setup.PrevPosition = item1->Pose.Position;
	}
	else
	{
		item1->Pose.Position.x = coll->Setup.PrevPosition.x;
		item1->Pose.Position.z = coll->Setup.PrevPosition.z;
	}

	// If player is interacting with an item, cancel it.
	if (item1->IsLara())
	{
		auto& player = GetLaraInfo(*item1);

		if (player.Control.Count.PositionAdjust > (PLAYER_POSITION_ADJUST_MAX_TIME / 6))
		{
			player.Control.IsMoving = false;
			player.Control.HandStatus = HandStatus::Free;
		}
	}

	return true;
}

// Simplified version of ItemPushItem for basic pushes.
bool ItemPushItem(ItemInfo* item, ItemInfo* item2)
{
	float sinY = phd_sin(item->Pose.Orientation.y);
	float cosY = phd_cos(item->Pose.Orientation.y);

	// Get direction vector from item to player.
	auto direction = item2->Pose.Position - item->Pose.Position;

	// Rotate Lara vector into item frame.
	int rx = (direction.x * cosY) - (direction.z * sinY);
	int rz = (direction.z * cosY) + (direction.x * sinY);

	const auto& anim = GetAnimData(*item);
	const auto& keyframe = anim.GetClosestKeyframe(item->Animation.FrameNumber);
	const auto& bounds = keyframe.BoundingBox;

	int minX = bounds.X1;
	int maxX = bounds.X2;
	int minZ = bounds.Z1;
	int maxZ = bounds.Z2;

	// Big enemies
	if (abs(direction.x) > BLOCK(4.5f) || abs(direction.z) > BLOCK(4.5f) ||
		rx <= minX || rx >= maxX ||
		rz <= minZ || rz >= maxZ)
	{
		return false;
	}

	int left = rx - minX;
	int top = maxZ - rz;
	int bottom = rz - minZ;
	int right = maxX - rx;

	if (right <= left && right <= top && right <= bottom)
		rx += right;
	else if (left <= right && left <= top && left <= bottom)
		rx -= left;
	else if (top <= left && top <= right && top <= bottom)
		rz += top;
	else
		rz -= bottom;

	item2->Pose.Position.x = item->Pose.Position.x + (rx * cosY) + (rz * sinY);
	item2->Pose.Position.z = item->Pose.Position.z + (rz * cosY) - (rx * sinY);

	return true;
}

// NOTE: Previously ItemPushLaraStatic().
bool ItemPushStatic(ItemInfo* item, const MESH_INFO& mesh, CollisionInfo* coll)
{
	const auto& bounds = GetBoundsAccurate(mesh, false);

	float sinY = phd_sin(mesh.pos.Orientation.y);
	float cosY = phd_cos(mesh.pos.Orientation.y);
	
	auto direction = item->Pose.Position - mesh.pos.Position;
	auto dz = item->Pose.Position.z - mesh.pos.Position.z;
	auto rx = (direction.x * cosY) - (direction.z * sinY);
	auto rz = (direction.z * cosY) + (direction.x * sinY);
	auto minX = bounds.X1 - coll->Setup.Radius;
	auto maxX = bounds.X2 + coll->Setup.Radius;
	auto minZ = bounds.Z1 - coll->Setup.Radius;
	auto maxZ = bounds.Z2 + coll->Setup.Radius;

	if (abs(direction.x) > BLOCK(4.5f) || abs(direction.z) > BLOCK(4.5f) ||
		rx <= minX || rx >= maxX ||
		rz <= minZ || rz >= maxZ)
	{
		return false;
	}

	auto left = rx - minX;
	auto top = maxZ - rz;
	auto bottom = rz - minZ;
	auto right = maxX - rx;

	if (right <= left && right <= top && right <= bottom)
		rx += right;
	else if (left <= right && left <= top && left <= bottom)
		rx -= left;
	else if (top <= left && top <= right && top <= bottom)
		rz += top;
	else
		rz -= bottom;

	item->Pose.Position.x = mesh.pos.Position.x + cosY * rx + sinY * rz;
	item->Pose.Position.z = mesh.pos.Position.z + cosY * rz - sinY * rx;

	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;

	auto prevHeadingAngle = coll->Setup.ForwardAngle;
	coll->Setup.ForwardAngle = phd_atan(item->Pose.Position.z - coll->Setup.PrevPosition.z, item->Pose.Position.x - coll->Setup.PrevPosition.x);

	GetCollisionInfo(coll, item);

	coll->Setup.ForwardAngle = prevHeadingAngle;

	if (coll->CollisionType == CollisionType::None)
	{
		coll->Setup.PrevPosition = item->Pose.Position;
		if (item->IsLara())
			UpdateLaraRoom(item, -10);
	}
	else
	{
		item->Pose.Position.x = coll->Setup.PrevPosition.x;
		item->Pose.Position.z = coll->Setup.PrevPosition.z;
	}

	// If Lara is in the process of aligning to an object, cancel it.
	if (item->IsLara() && Lara.Control.IsMoving && Lara.Control.Count.PositionAdjust > (PLAYER_POSITION_ADJUST_MAX_TIME / 6))
	{
		auto* lara = GetLaraInfo(item);
		lara->Control.IsMoving = false;
		lara->Control.HandStatus = HandStatus::Free;
	}

	return true;
}

void ItemPushBridge(ItemInfo& item, CollisionInfo& coll)
{
	coll.Setup.ForwardAngle = item.Pose.Orientation.y;
	coll.Setup.PrevPosition = item.Pose.Position;
	GetCollisionInfo(&coll, &item);
	ShiftItem(&item, &coll);
}

void CollideBridgeItems(ItemInfo& item, CollisionInfo& coll, PointCollisionData& pointColl)
{
	// Store offset for bridge item into shifts if it exists.
	if (coll.LastBridgeItemNumber == pointColl.GetFloorBridgeItemNumber() && coll.LastBridgeItemNumber != NO_VALUE)
	{
		auto& bridgeItem = g_Level.Items[pointColl.GetFloorBridgeItemNumber()];

		auto deltaPos = bridgeItem.Pose.Position - coll.LastBridgeItemPose.Position;
		auto deltaOrient = bridgeItem.Pose.Orientation - coll.LastBridgeItemPose.Orientation;
		auto deltaPose = Pose(deltaPos, deltaOrient);

		// Item is grounded and bridge position changed; set shift.
		if (deltaPose != Pose::Zero && !item.Animation.IsAirborne &&
			item.IsLara() ? (GetLaraInfo(item).Control.WaterStatus != WaterStatus::Underwater && GetLaraInfo(item).Control.WaterStatus != WaterStatus::FlyCheat) : true)
		{
			const auto& bridgePos = bridgeItem.Pose.Position;

			// Calculate offset.
			auto relOffset = (item.Pose.Position - bridgePos).ToVector3();
			auto rotMatrix = deltaPose.Orientation.ToRotationMatrix();
			auto offset = bridgePos.ToVector3() + Vector3::Transform(relOffset, rotMatrix);

			deltaPose.Position -= item.Pose.Position - Vector3i(offset);

			// Don't update shifts if difference is too big (bridge was possibly teleported or just entered).
			if (Vector2(deltaPose.Position.x, deltaPose.Position.z).Length() <= (coll.Setup.Radius * 2))
			{
				deltaPose.Orientation = EulerAngles(0, deltaPose.Orientation.y, 0);
				coll.Shift = deltaPose;
			}
		}
		// Push item.
		else if (TestBoundsCollide(&bridgeItem, &item, coll.Setup.Radius) &&
			Vector2(deltaPose.Position.x, deltaPose.Position.z).Length() <= coll.Setup.Radius &&
			(deltaPos != Vector3i::Zero || deltaOrient != EulerAngles::Identity))
		{
			ItemPushItem(&bridgeItem, &item);
		}

		coll.LastBridgeItemPose = bridgeItem.Pose;
	}
	else
	{
		coll.LastBridgeItemPose = Pose::Zero;
		coll.LastBridgeItemNumber = NO_VALUE;
	}

	coll.LastBridgeItemNumber = pointColl.GetFloorBridgeItemNumber();
}

void CollideSolidStatics(ItemInfo* item, CollisionInfo* coll)
{
	coll->HitStatic = false;
	coll->HitTallObject = false;

	for (auto i : g_Level.Rooms[item->RoomNumber].NeighborRoomNumbers)
	{
		if (!g_Level.Rooms[i].Active())
			continue;

		for (auto& mesh : g_Level.Rooms[i].mesh)
		{
			// Only process meshes which are visible.
			if (!(mesh.flags & StaticMeshFlags::SM_VISIBLE))
				continue;

			// Bypass static meshes which are marked as non-collidable.
			if (!(mesh.flags & StaticMeshFlags::SM_COLLISION))
				continue;

			// Only process meshes which are solid, or if solid mode is set by the setup.
			if (!coll->Setup.ForceSolidStatics && !(mesh.flags & StaticMeshFlags::SM_SOLID))
				continue;

			float distance = Vector3i::Distance(item->Pose.Position, mesh.pos.Position);
			if (distance < COLLISION_CHECK_DISTANCE)
			{
				if (CollideSolidBounds(item, GetBoundsAccurate(mesh, false), mesh.pos, coll))
					coll->HitStatic = true;
			}
		}
	}
}

bool CollideSolidBounds(ItemInfo* item, const GameBoundingBox& box, const Pose& pose, CollisionInfo* coll)
{
	bool result = false;

	// Ignore processing null bounds.
	if (box.GetExtents().Length() <= COLLIDABLE_BOUNDS_THRESHOLD)
		return false;

	// Get DX static bounds in global coordinates.
	auto staticBounds = box.ToBoundingOrientedBox(pose);

	// Get local TR bounds and DX item bounds in global coordinates.
	auto itemBBox = GameBoundingBox(item);
	auto itemBounds = itemBBox.ToBoundingOrientedBox(item->Pose);

	// Extend bounds a bit for visual testing.
	itemBounds.Extents = itemBounds.Extents + Vector3(BLOCK(1));

	// Filter out any further checks if static isn't nearby.
	if (!staticBounds.Intersects(itemBounds))
		return false;

	// Bring back extents.
	itemBounds.Extents = itemBounds.Extents - Vector3(BLOCK(1));

	// Draw static bounds.
	DrawDebugBox(staticBounds, Vector4(1, 0.3f, 0, 1), RendererDebugPage::CollisionStats);

	// Calculate horizontal item collision bounds according to radius.
	GameBoundingBox collBox;
	collBox.X1 = -coll->Setup.Radius;
	collBox.X2 = coll->Setup.Radius;
	collBox.Z1 = -coll->Setup.Radius;
	collBox.Z2 = coll->Setup.Radius;

	// Calculate vertical item collision bounds according to either height (land mode) or precise bounds (water mode).
	// Water mode needs special processing because height calculation in original engines is inconsistent in such cases.
	if (TestEnvironment(ENV_FLAG_WATER, item))
	{
		collBox.Y1 = itemBBox.Y1;
		collBox.Y2 = itemBBox.Y2;
	}
	else
	{
		collBox.Y1 = -coll->Setup.Height;
		collBox.Y2 = 0;
	}

	// Get and test DX item collision bounds.
	auto collBounds = collBox.ToBoundingOrientedBox(Pose(item->Pose.Position));
	bool intersects = staticBounds.Intersects(collBounds);

	// Draw item coll bounds.
	DrawDebugBox(collBounds, intersects ? Vector4(1, 0, 0, 1) : Vector4(0, 1, 0, 1), RendererDebugPage::CollisionStats);

	// Decompose static bounds into top/bottom plane vertices.
	Vector3 corners[8];
	staticBounds.GetCorners(corners);
	Vector3 planeVertices[4][3] =
	{
		{ corners[0], corners[4], corners[1] },
		{ corners[5], corners[4], corners[1] },
		{ corners[3], corners[6], corners[7] },
		{ corners[3], corners[6], corners[2] }
	};

	// Determine collision box vertical dimensions.
	auto height = collBox.GetHeight();
	auto center = item->Pose.Position.y - (height / 2);

	// Do a series of angular tests with 90 degree steps to determine top/bottom collision.
	int closestPlane = -1;
	auto closestRay = Ray();
	auto minDistance = FLT_MAX;
	for (int i = 0; i < 4; i++)
	{
		// Calculate ray direction.
		auto mxR = Matrix::CreateFromYawPitchRoll(TO_RAD(item->Pose.Orientation.y), TO_RAD(item->Pose.Orientation.x + (ANGLE(90 * i))), 0.0f);
		auto mxT = Matrix::CreateTranslation(Vector3::UnitY);
		auto direction = (mxT * mxR).Translation();

		// Make a ray and do ray tests against all decomposed planes.
		auto ray = Ray(collBounds.Center, direction);

		// Determine if top/bottom planes are closest ones or not.
		for (int p = 0; p < 4; p++)
		{
			// No plane intersection, quickly discard.
			float d = 0.0f;
			if (!ray.Intersects(planeVertices[p][0], planeVertices[p][1], planeVertices[p][2], d))
				continue;

			// Process plane intersection only if distance is smaller than already found minimum.
			if (d < minDistance)
			{
				closestRay = ray;
				closestPlane = p;
				minDistance = d;
			}
		}
	}

	// Top/bottom plane found.
	if (closestPlane != -1)
	{
		auto bottom = closestPlane >= 2;
		auto yPoint = abs((closestRay.direction * minDistance).y);
		auto distanceToVerticalPlane = height / 2 - yPoint;

		// Correct position according to top/bottom bounds, if collided and plane is nearby.
		if (intersects && minDistance < height)
		{
			if (bottom)
			{
				// HACK: Additionally subtract 2 from bottom plane, otherwise false positives may occur.
				item->Pose.Position.y += distanceToVerticalPlane + 2;
				coll->CollisionType = CollisionType::Top;
			}
			else
			{
				// Set collision type only if dry room (in water rooms the player can get stuck).
				item->Pose.Position.y -= distanceToVerticalPlane;
				coll->CollisionType = (g_Level.Rooms[item->RoomNumber].flags & 1) ? coll->CollisionType : CollisionType::Clamp;
			}

			result = true;
		}

		if (bottom && coll->Middle.Ceiling < distanceToVerticalPlane)
			coll->Middle.Ceiling = distanceToVerticalPlane;
	}

	// If no actual intersection occured, stop testing.
	if (!intersects)
		return false;

	// Check if bounds still collide after top/bottom position correction.
	if (!staticBounds.Intersects(collBox.ToBoundingOrientedBox(Pose(item->Pose.Position))))
		return result;

	// Determine identity orientation/distance.
	auto distance = (item->Pose.Position - pose.Position).ToVector3();
	auto sinY = phd_sin(pose.Orientation.y);
	auto cosY = phd_cos(pose.Orientation.y);

	// Rotate item to collision bounds identity.
	auto x = round((distance.x * cosY) - (distance.z * sinY)) + pose.Position.x;
	auto y = item->Pose.Position.y;
	auto z = round((distance.x * sinY) + (distance.z * cosY)) + pose.Position.z;

	// Determine identity static collision bounds.
	auto xMin = pose.Position.x + box.X1;
	auto xMax = pose.Position.x + box.X2;
	auto yMin = pose.Position.y + box.Y1;
	auto yMax = pose.Position.y + box.Y2;
	auto zMin = pose.Position.z + box.Z1;
	auto zMax = pose.Position.z + box.Z2;

	// Determine item collision bounds.
	auto inXMin = x + collBox.X1;
	auto inXMax = x + collBox.X2;
	auto inYMin = y + collBox.Y1;
	auto inYMax = y + collBox.Y2;
	auto inZMin = z + collBox.Z1;
	auto inZMax = z + collBox.Z2;

	// Don't calculate shifts if not in bounds.
	if (inXMax <= xMin || inXMin >= xMax ||
		inYMax <= yMin || inYMin >= yMax ||
		inZMax <= zMin || inZMin >= zMax)
	{
		return result;
	}

	// Calculate shifts.

	auto rawShift = Vector3i::Zero;
	auto shiftLeft = inXMax - xMin;
	auto shiftRight = xMax - inXMin;

	if (shiftLeft < shiftRight)
		rawShift.x = -shiftLeft;
	else
		rawShift.x = shiftRight;

	shiftLeft = inZMax - zMin;
	shiftRight = zMax - inZMin;

	if (shiftLeft < shiftRight)
		rawShift.z = -shiftLeft;
	else
		rawShift.z = shiftRight;

	// Rotate previous collision position to identity.
	distance = (coll->Setup.PrevPosition - pose.Position).ToVector3();
	auto ox = round((distance.x * cosY) - (distance.z * sinY)) + pose.Position.x;
	auto oz = round((distance.x * sinY) + (distance.z * cosY)) + pose.Position.z;

	// Calculate collisison type based on identity orientation.
	switch (GetQuadrant(coll->Setup.ForwardAngle - pose.Orientation.y))
	{
	case NORTH:
		if (rawShift.x > coll->Setup.Radius || rawShift.x < -coll->Setup.Radius)
		{
			coll->Shift.Position.z = rawShift.z;
			coll->Shift.Position.x = ox - x;
			coll->CollisionType = CollisionType::Front;
		}
		else if (rawShift.x > 0 && rawShift.x <= coll->Setup.Radius)
		{
			coll->Shift.Position.x = rawShift.x;
			coll->Shift.Position.z = 0;
			coll->CollisionType = CollisionType::Left;
		}
		else if (rawShift.x < 0 && rawShift.x >= -coll->Setup.Radius)
		{
			coll->Shift.Position.x = rawShift.x;
			coll->Shift.Position.z = 0;
			coll->CollisionType = CollisionType::Right;
		}

		break;

	case SOUTH:
		if (rawShift.x > coll->Setup.Radius || rawShift.x < -coll->Setup.Radius)
		{
			coll->Shift.Position.z = rawShift.z;
			coll->Shift.Position.x = ox - x;
			coll->CollisionType = CollisionType::Front;
		}
		else if (rawShift.x > 0 && rawShift.x <= coll->Setup.Radius)
		{
			coll->Shift.Position.x = rawShift.x;
			coll->Shift.Position.z = 0;
			coll->CollisionType = CollisionType::Right;
		}
		else if (rawShift.x < 0 && rawShift.x >= -coll->Setup.Radius)
		{
			coll->Shift.Position.x = rawShift.x;
			coll->Shift.Position.z = 0;
			coll->CollisionType = CollisionType::Left;
		}

		break;

	case EAST:
		if (rawShift.z > coll->Setup.Radius || rawShift.z < -coll->Setup.Radius)
		{
			coll->Shift.Position.x = rawShift.x;
			coll->Shift.Position.z = oz - z;
			coll->CollisionType = CollisionType::Front;
		}
		else if (rawShift.z > 0 && rawShift.z <= coll->Setup.Radius)
		{
			coll->Shift.Position.z = rawShift.z;
			coll->Shift.Position.x = 0;
			coll->CollisionType = CollisionType::Right;
		}
		else if (rawShift.z < 0 && rawShift.z >= -coll->Setup.Radius)
		{
			coll->Shift.Position.z = rawShift.z;
			coll->Shift.Position.x = 0;
			coll->CollisionType = CollisionType::Left;
		}

		break;

	case WEST:
		if (rawShift.z > coll->Setup.Radius || rawShift.z < -coll->Setup.Radius)
		{
			coll->Shift.Position.x = rawShift.x;
			coll->Shift.Position.z = oz - z;
			coll->CollisionType = CollisionType::Front;
		}
		else if (rawShift.z > 0 && rawShift.z <= coll->Setup.Radius)
		{
			coll->Shift.Position.z = rawShift.z;
			coll->Shift.Position.x = 0;
			coll->CollisionType = CollisionType::Left;
		}
		else if (rawShift.z < 0 && rawShift.z >= -coll->Setup.Radius)
		{
			coll->Shift.Position.z = rawShift.z;
			coll->Shift.Position.x = 0;
			coll->CollisionType = CollisionType::Right;
		}

		break;
	}

	// Determine final shifts orientation/distance.
	distance = Vector3(x + coll->Shift.Position.x, y, z + coll->Shift.Position.z) - pose.Position.ToVector3();
	sinY = phd_sin(-pose.Orientation.y);
	cosY = phd_cos(-pose.Orientation.y);

	// Calculate final shifts orientation/distance.
	coll->Shift.Position.x = (round((distance.x * cosY) - (distance.z * sinY)) + pose.Position.x) - item->Pose.Position.x;
	coll->Shift.Position.z = (round((distance.x * sinY) + (distance.z * cosY)) + pose.Position.z) - item->Pose.Position.z;

	if (coll->Shift.Position.x == 0 && coll->Shift.Position.z == 0)
		coll->CollisionType = CollisionType::None; // Paranoid.

	// Set splat state flag if item is Lara and bounds are taller than Lara's headroom.
	if (item == LaraItem && coll->CollisionType == CollisionType::Front)
		coll->HitTallObject = (yMin <= inYMin + LARA_HEADROOM);

	return true;
}

// NOTE: Previously DoProperDetection().
void DoProjectileDynamics(short itemNumber, int x, int y, int z, int xv, int yv, int zv)
{
	auto* item = &g_Level.Items[itemNumber];

	auto prevPointColl = GetPointCollision(Vector3i(x, y, z), item->RoomNumber);
	auto pointColl = GetPointCollision(*item);

	// TODO: Use floor normal directly.
	auto floorTilt = GetSurfaceTilt(pointColl.GetFloorNormal(), true);

	auto bounds = GameBoundingBox(item);
	int radius = bounds.GetHeight();

	item->Pose.Position.y += radius;

	if (item->Pose.Position.y >= pointColl.GetFloorHeight())
	{
		int bs = 0;

		if (pointColl.IsSteepFloor() && prevPointColl.GetFloorHeight() < pointColl.GetFloorHeight())
		{
			int yAngle = (long)((unsigned short)item->Pose.Orientation.y);

			if (floorTilt.x < 0)
			{
				if (yAngle >= ANGLE(180.0f))
					bs = 1;
			}
			else if (floorTilt.x > 0)
			{
				if (yAngle <= ANGLE(180.0f))
					bs = 1;
			}

			if (floorTilt.y < 0)
			{
				if (yAngle >= ANGLE(90.0f) && yAngle <= ANGLE(270.0f))
					bs = 1;
			}
			else if (floorTilt.y > 0)
			{
				if (yAngle <= ANGLE(90.0f) || yAngle >= ANGLE(270.0f))
					bs = 1;
			}
		}

		// If last position of item was also below this floor height, we've hit a wall, else we've hit a floor.

		if (y > (pointColl.GetFloorHeight() + 32) && bs == 0 &&
			(((x / BLOCK(1)) != (item->Pose.Position.x / BLOCK(1))) ||
				((z / BLOCK(1)) != (item->Pose.Position.z / BLOCK(1)))))
		{
			// Need to know which direction the wall is.

			long xs;

			if ((x & (~WALL_MASK)) != (item->Pose.Position.x & (~WALL_MASK)) &&	// X crossed boundary?
				(z & (~WALL_MASK)) != (item->Pose.Position.z & (~WALL_MASK)))	// Z crossed boundary as well?
			{
				if (abs(x - item->Pose.Position.x) < abs(z - item->Pose.Position.z))
					xs = 1;	// X has travelled the shortest, so (maybe) hit first. (Seems to work ok).
				else
					xs = 0;
			}
			else
				xs = 1;

			if ((x & (~WALL_MASK)) != (item->Pose.Position.x & (~WALL_MASK)) && xs)	// X crossed boundary?
			{
				// Hit angle = ANGLE(270.0f).
				if (xv <= 0)
					item->Pose.Orientation.y = ANGLE(90.0f) + (ANGLE(270.0f) - item->Pose.Orientation.y);
				// Hit angle = 0x4000.
				else
					item->Pose.Orientation.y = ANGLE(270.0f) + (ANGLE(90.0f) - item->Pose.Orientation.y);
			}
			// Z crossed boundary.
			else
				item->Pose.Orientation.y = ANGLE(180.0f) - item->Pose.Orientation.y;

			item->Animation.Velocity.z /= 2;

			// Put item back in its last position.
			item->Pose.Position.x = x;
			item->Pose.Position.y = y;
			item->Pose.Position.z = z;
		}
		// Hit a steep slope?
		else if (pointColl.IsSteepFloor())
		{
			// Need to know which direction the slope is.

			item->Animation.Velocity.z -= (item->Animation.Velocity.z / 4);

			// Hit angle = ANGLE(90.0f)
			if (floorTilt.x < 0 && ((abs(floorTilt.x)) - (abs(floorTilt.y)) >= 2))
			{
				if (((unsigned short)item->Pose.Orientation.y) > ANGLE(180.0f))
				{
					item->Pose.Orientation.y = ANGLE(90.0f) + (ANGLE(270.0f) - (unsigned short)item->Pose.Orientation.y - 1);
					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
				}
				else
				{
					if (item->Animation.Velocity.z < 32)
					{
						item->Animation.Velocity.z -= floorTilt.x * 2;
						if ((unsigned short)item->Pose.Orientation.y > ANGLE(90.0f) && (unsigned short)item->Pose.Orientation.y < ANGLE(270.0f))
						{
							item->Pose.Orientation.y -= ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y < ANGLE(90.0f))
								item->Pose.Orientation.y = ANGLE(90.0f);
						}
						else if ((unsigned short)item->Pose.Orientation.y < ANGLE(90.0f))
						{
							item->Pose.Orientation.y += ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y > ANGLE(90.0f))
								item->Pose.Orientation.y = ANGLE(90.0f);
						}
					}

					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
					else
						item->Animation.Velocity.y = 0;
				}
			}
			// Hit angle = ANGLE(270.0f)
			else if (floorTilt.x > 0 && ((abs(floorTilt.x)) - (abs(floorTilt.y)) >= 2))
			{
				if (((unsigned short)item->Pose.Orientation.y) < ANGLE(180.0f))
				{
					item->Pose.Orientation.y = ANGLE(270.0f) + (ANGLE(90.0f) - (unsigned short)item->Pose.Orientation.y - 1);
					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
				}
				else
				{
					if (item->Animation.Velocity.z < 32)
					{
						item->Animation.Velocity.z += floorTilt.x * 2;
						if ((unsigned short)item->Pose.Orientation.y > ANGLE(270.0f) || (unsigned short)item->Pose.Orientation.y < ANGLE(90.0f))
						{
							item->Pose.Orientation.y -= ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y < ANGLE(270.0f))
								item->Pose.Orientation.y = ANGLE(270.0f);
						}
						else if ((unsigned short)item->Pose.Orientation.y < ANGLE(270.0f))
						{
							item->Pose.Orientation.y += ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y > ANGLE(270.0f))
								item->Pose.Orientation.y = ANGLE(270.0f);
						}
					}

					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
					else
						item->Animation.Velocity.y = 0;
				}
			}
			// Hit angle = 0
			else if (floorTilt.y < 0 && ((abs(floorTilt.y)) - (abs(floorTilt.x)) >= 2))
			{
				if (((unsigned short)item->Pose.Orientation.y) > ANGLE(90.0f) && ((unsigned short)item->Pose.Orientation.y) < ANGLE(270.0f))
				{
					item->Pose.Orientation.y = ANGLE(180.0f) - item->Pose.Orientation.y - 1;
					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
				}
				else
				{
					if (item->Animation.Velocity.z < 32)
					{
						item->Animation.Velocity.z -= floorTilt.y * 2;

						if ((unsigned short)item->Pose.Orientation.y < ANGLE(180.0f))
						{
							item->Pose.Orientation.y -= ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y > ANGLE(337.5))
								item->Pose.Orientation.y = 0;
						}
						else if ((unsigned short)item->Pose.Orientation.y >= ANGLE(180.0f))
						{
							item->Pose.Orientation.y += ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y < ANGLE(22.5f))
								item->Pose.Orientation.y = 0;
						}
					}

					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
					else
						item->Animation.Velocity.y = 0;
				}
			}
			// Hit angle = ANGLE(180.0f)
			else if (floorTilt.y > 0 && ((abs(floorTilt.y)) - (abs(floorTilt.x)) >= 2))
			{
				if (((unsigned short)item->Pose.Orientation.y) > ANGLE(270.0f) || ((unsigned short)item->Pose.Orientation.y) < ANGLE(90.0f))
				{
					item->Pose.Orientation.y = ANGLE(180.0f) - item->Pose.Orientation.y - 1;
					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
				}
				else
				{
					if (item->Animation.Velocity.z < 32)
					{
						item->Animation.Velocity.z += floorTilt.y * 2;

						if ((unsigned short)item->Pose.Orientation.y > ANGLE(180.0f))
						{
							item->Pose.Orientation.y -= ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y < ANGLE(180.0f))
								item->Pose.Orientation.y = ANGLE(180.0f);
						}
						else if ((unsigned short)item->Pose.Orientation.y < ANGLE(180.0f))
						{
							item->Pose.Orientation.y += ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y > ANGLE(180.0f))
								item->Pose.Orientation.y = ANGLE(180.0f);
						}
					}

					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
					else
						item->Animation.Velocity.y = 0;
				}
			}
			else if (floorTilt.x < 0 && floorTilt.y < 0)	// Hit angle = 0x2000
			{
				if (((unsigned short)item->Pose.Orientation.y) > ANGLE(135.0f) && ((unsigned short)item->Pose.Orientation.y) < ANGLE(315.0f))
				{
					item->Pose.Orientation.y = ANGLE(45.0f) + (ANGLE(225.0f) - (unsigned short)item->Pose.Orientation.y - 1);
					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
				}
				else
				{
					if (item->Animation.Velocity.z < 32)
					{
						item->Animation.Velocity.z += -floorTilt.x + -floorTilt.y;
						if ((unsigned short)item->Pose.Orientation.y > ANGLE(45.0f) && (unsigned short)item->Pose.Orientation.y < ANGLE(225.0f))
						{
							item->Pose.Orientation.y -= ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y < ANGLE(45.0f))
								item->Pose.Orientation.y = ANGLE(45.0f);
						}
						else if ((unsigned short)item->Pose.Orientation.y != ANGLE(45.0f))
						{
							item->Pose.Orientation.y += ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y > ANGLE(45.0f))
								item->Pose.Orientation.y = ANGLE(45.0f);
						}
					}

					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
					else
						item->Animation.Velocity.y = 0;
				}
			}
			// Hit angle = ANGLE(135.0f)
			else if (floorTilt.x < 0 && floorTilt.y > 0)
			{
				if (((unsigned short)item->Pose.Orientation.y) > ANGLE(225.0f) || ((unsigned short)item->Pose.Orientation.y) < ANGLE(45.0f))
				{
					item->Pose.Orientation.y = ANGLE(135.0f) + (ANGLE(315.0f) - (unsigned short)item->Pose.Orientation.y - 1);
					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
				}
				else
				{
					if (item->Animation.Velocity.z < 32)
					{
						item->Animation.Velocity.z += (-floorTilt.x) + floorTilt.y;
						if ((unsigned short)item->Pose.Orientation.y < ANGLE(315.0f) && (unsigned short)item->Pose.Orientation.y > ANGLE(135.0f))
						{
							item->Pose.Orientation.y -= ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y < ANGLE(135.0f))
								item->Pose.Orientation.y = ANGLE(135.0f);
						}
						else if ((unsigned short)item->Pose.Orientation.y != ANGLE(135.0f))
						{
							item->Pose.Orientation.y += ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y > ANGLE(135.0f))
								item->Pose.Orientation.y = ANGLE(135.0f);
						}
					}

					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
					else
						item->Animation.Velocity.y = 0;
				}
			}
			// Hit angle = ANGLE(225.5f)
			else if (floorTilt.x > 0 && floorTilt.y > 0)
			{
				if (((unsigned short)item->Pose.Orientation.y) > ANGLE(315.0f) || ((unsigned short)item->Pose.Orientation.y) < ANGLE(135.0f))
				{
					item->Pose.Orientation.y = ANGLE(225.5f) + (ANGLE(45.0f) - (unsigned short)item->Pose.Orientation.y - 1);
					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
				}
				else
				{
					if (item->Animation.Velocity.z < 32)
					{
						item->Animation.Velocity.z += floorTilt.x + floorTilt.y;
						if ((unsigned short)item->Pose.Orientation.y < ANGLE(45.0f) || (unsigned short)item->Pose.Orientation.y > ANGLE(225.5f))
						{
							item->Pose.Orientation.y -= ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y < ANGLE(225.5f))
								item->Pose.Orientation.y = ANGLE(225.5f);
						}
						else if ((unsigned short)item->Pose.Orientation.y != ANGLE(225.5f))
						{
							item->Pose.Orientation.y += ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y > ANGLE(225.5f))
								item->Pose.Orientation.y = ANGLE(225.5f);
						}
					}

					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
					else
						item->Animation.Velocity.y = 0;
				}
			}
			// Hit angle = ANGLE(315.0f)
			else if (floorTilt.x > 0 && floorTilt.y < 0)
			{
				if (((unsigned short)item->Pose.Orientation.y) > ANGLE(45.0f) && ((unsigned short)item->Pose.Orientation.y) < ANGLE(225.5f))
				{
					item->Pose.Orientation.y = ANGLE(315.0f) + (ANGLE(135.0f)- (unsigned short)item->Pose.Orientation.y - 1);
					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -item->Animation.Velocity.y / 2;
				}
				else
				{
					if (item->Animation.Velocity.z < 32)
					{
						item->Animation.Velocity.z += floorTilt.x + (-floorTilt.y);
						if ((unsigned short)item->Pose.Orientation.y < ANGLE(135.0f) || (unsigned short)item->Pose.Orientation.y > ANGLE(315.0f))
						{
							item->Pose.Orientation.y -= ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y < ANGLE(315.0f))
								item->Pose.Orientation.y = ANGLE(315.0f);
						}
						else if ((unsigned short)item->Pose.Orientation.y != ANGLE(315.0f))
						{
							item->Pose.Orientation.y += ANGLE(22.5f);
							if ((unsigned short)item->Pose.Orientation.y > ANGLE(315.0f))
								item->Pose.Orientation.y = ANGLE(315.0f);
						}
					}

					if (item->Animation.Velocity.y > 0)
						item->Animation.Velocity.y = -(item->Animation.Velocity.y / 2);
					else
						item->Animation.Velocity.y = 0;
				}
			}

			// Move item back to its previous position.
			item->Pose.Position.x = x;
			item->Pose.Position.y = y;
			item->Pose.Position.z = z;
		}
		else
		{
			// Hit the floor; bounce and slow down.
			if (item->Animation.Velocity.y > 0)
			{
				if (item->Animation.Velocity.y > 16)
				{
					if (item->ObjectNumber == ID_GRENADE)
						item->Animation.Velocity.y = -(item->Animation.Velocity.y - (item->Animation.Velocity.y / 2));
					else
					{
						item->Animation.Velocity.y = -(item->Animation.Velocity.y / 2);
						if (item->Animation.Velocity.y < -100)
							item->Animation.Velocity.y = -100;
					}
				}
				else
				{
					// Roll on floor.
					item->Animation.Velocity.y = 0;
					if (item->ObjectNumber == ID_GRENADE)
					{
						item->Animation.RequiredState = 1;
						item->Pose.Orientation.x = 0;
						item->Animation.Velocity.z--;
					}
					else
						item->Animation.Velocity.z -= 3;

					if (item->Animation.Velocity.z < 0)
						item->Animation.Velocity.z = 0;
				}
			}

			item->Pose.Position.y = pointColl.GetFloorHeight();
		}
	}
	// Check for on top of object.
	else
	{
		if (yv >= 0)
		{
			prevPointColl = GetPointCollision(Vector3i(item->Pose.Position.x, y, item->Pose.Position.z), item->RoomNumber);
			pointColl = GetPointCollision(*item);

			// Bounce off floor.

			// Removed weird OnObject global check from here which didnt make sense because OnObject
			// was always set to 0 by GetHeight() function which was called before the check.
			// Possibly a mistake or unfinished feature by Core? -- Lwmte, 27.08.21

			if (item->Pose.Position.y >= prevPointColl.GetFloorHeight())
			{
				// Hit the floor; bounce and slow down.
				if (item->Animation.Velocity.y > 0)
				{
					if (item->Animation.Velocity.y > 16)
					{
						if (item->ObjectNumber == ID_GRENADE)
							item->Animation.Velocity.y = -(item->Animation.Velocity.y - (item->Animation.Velocity.y / 2));
						else
						{
							item->Animation.Velocity.y = -(item->Animation.Velocity.y / 4);
							if (item->Animation.Velocity.y < -100)
								item->Animation.Velocity.y = -100;
						}
					}
					else
					{
						// Roll on floor.
						item->Animation.Velocity.y = 0;
						if (item->ObjectNumber == ID_GRENADE)
						{
							item->Animation.RequiredState = 1;
							item->Pose.Orientation.x = 0;
							item->Animation.Velocity.z--;
						}
						else
							item->Animation.Velocity.z -= 3;

						if (item->Animation.Velocity.z < 0)
							item->Animation.Velocity.z = 0;
					}
				}

				item->Pose.Position.y = prevPointColl.GetFloorHeight();
			}
		}
		// else
		{
			// Bounce off ceiling.
			pointColl = GetPointCollision(*item);

			if (item->Pose.Position.y < pointColl.GetCeilingHeight())
			{
				if (y < pointColl.GetCeilingHeight() &&
					(((x / BLOCK(1)) != (item->Pose.Position.x / BLOCK(1))) ||
						((z / BLOCK(1)) != (item->Pose.Position.z / BLOCK(1)))))
				{
					// Need to know which direction the wall is.

					// X crossed boundary?
					if ((x & ~WALL_MASK) != (item->Pose.Position.x & ~WALL_MASK))
					{
						if (xv <= 0)	// Hit angle = ANGLE(270.0f).
							item->Pose.Orientation.y = ANGLE(90.0f) + (ANGLE(270.0f) - item->Pose.Orientation.y);
						else			// Hit angle = ANGLE(90.0f).
							item->Pose.Orientation.y = ANGLE(270.0f) + (ANGLE(90.0f) - item->Pose.Orientation.y);
					}
					// Z crossed boundary.
					else
						item->Pose.Orientation.y = 0x8000 - item->Pose.Orientation.y;

					if (item->ObjectNumber == ID_GRENADE)
						item->Animation.Velocity.z -= item->Animation.Velocity.z / 8;
					else
						item->Animation.Velocity.z /= 2;

					// Move item back to its previous position.
					item->Pose.Position.x = x;
					item->Pose.Position.y = y;
					item->Pose.Position.z = z;
				}
				else
					item->Pose.Position.y = pointColl.GetCeilingHeight();

				if (item->Animation.Velocity.y < 0)
					item->Animation.Velocity.y = -item->Animation.Velocity.y;
			}
		}
	}

	pointColl = GetPointCollision(*item);

	if (pointColl.GetRoomNumber() != item->RoomNumber)
	{
		if (item->ObjectNumber == ID_GRENADE && TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, pointColl.GetRoomNumber()))
			Splash(item);

		ItemNewRoom(itemNumber, pointColl.GetRoomNumber());
	}

	item->Pose.Position.y -= radius;
}

void DoObjectCollision(ItemInfo* item, CollisionInfo* coll)
{
	item->HitStatus = false;
	coll->HitStatic = false;

	bool isPlayer = item->IsLara();
	bool isHarmless = !isPlayer && (item->Data.is<KayakInfo>() || item->Data.is<UPVInfo>());

	if (isPlayer)
	{
		GetLaraInfo(*item).HitDirection = NO_VALUE;

		if (item->HitPoints <= 0)
			return;
	}

	if (Objects[item->ObjectNumber].intelligent)
		return;

	const auto& room = g_Level.Rooms[item->RoomNumber];
	for (int neighborRoomNumber : room.NeighborRoomNumbers)
	{
		auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
		if (!neighborRoom.Active())
			continue;

		int nextItemNumber = neighborRoom.itemNumber;
		while (nextItemNumber != NO_VALUE)
		{
			auto& linkItem = g_Level.Items[nextItemNumber];
			int itemNumber = nextItemNumber;

			// HACK: For some reason, sometimes an infinite loop may happen here.
			if (nextItemNumber == linkItem.NextItem)
				break;

			nextItemNumber = linkItem.NextItem;

			if (&linkItem == item)
				continue;

			if (!(linkItem.Collidable && linkItem.Status != ITEM_INVISIBLE))
				continue;

			const auto& object = Objects[linkItem.ObjectNumber];

			if (object.collision == nullptr)
				continue;

			if (Vector3i::Distance(linkItem.Pose.Position, item->Pose.Position) >= COLLISION_CHECK_DISTANCE)
				continue;

			if (isPlayer)
			{
				// Objects' own collision routines were almost universally written only for
				// managing collisions with Lara and nothing else. Until all of these routines
				// are refactored (which won't happen anytime soon), we need this differentiation.
				object.collision(itemNumber, item, coll);
			}
			else
			{
				if (!TestBoundsCollide(&linkItem, item, coll->Setup.Radius))
					continue;

				// Infer object is nullmesh or invisible object by valid draw routine.
				if (object.drawRoutine == nullptr)
					continue;

				// Pickups are also not processed.
				if (object.isPickup)
					continue;

				// If colliding object is an enemy, kill it.
				if (object.intelligent)
				{
					// Don't try killing already dead or non-targetable enemies.
					if (linkItem.HitPoints <= 0 || linkItem.HitPoints == NOT_TARGETABLE)
						continue;

					if (isHarmless || abs(item->Animation.Velocity.z) < VEHICLE_COLLISION_TERMINAL_VELOCITY)
					{
						// If vehicle is harmless or speed is too low, just push enemy.
						ItemPushItem(&linkItem, item, coll, false, 0);
						continue;
					}
					else
					{
						DoDamage(&linkItem, INT_MAX);
						DoLotsOfBlood(
							linkItem.Pose.Position.x,
							item->Pose.Position.y - CLICK(1),
							linkItem.Pose.Position.z,
							item->Animation.Velocity.z,
							item->Pose.Orientation.y,
							linkItem.RoomNumber, 3);
					}
				}
				else if (coll->Setup.EnableObjectPush)
				{
					ItemPushItem(&linkItem, item, coll, false, 1);
				}
			}
		}

		for (auto& staticObject : neighborRoom.mesh)
		{
			// Check if static is visible.
			if (!(staticObject.flags & StaticMeshFlags::SM_VISIBLE))
				continue;

			// Check if static is collidable.
			if (!(staticObject.flags & StaticMeshFlags::SM_COLLISION))
				continue;

			// For Lara, solid static mesh collisions are directly managed by GetCollisionInfo,
			// so we bypass them here to avoid interference.
			if (isPlayer && (staticObject.flags & StaticMeshFlags::SM_SOLID))
				continue;

			if (Vector3i::Distance(staticObject.pos.Position, item->Pose.Position) >= COLLISION_CHECK_DISTANCE)
				continue;

			if (!TestBoundsCollideStatic(item, staticObject, coll->Setup.Radius))
				continue;

			coll->HitStatic = true;

			// HACK: Shatter statics only by harmful vehicles.
			if (!isPlayer && 
				!isHarmless && abs(item->Animation.Velocity.z) > VEHICLE_COLLISION_TERMINAL_VELOCITY &&
				Statics[staticObject.staticNumber].shatterType != ShatterType::None)
			{
				SoundEffect(GetShatterSound(staticObject.staticNumber), &staticObject.pos);
				ShatterObject(nullptr, &staticObject, -128, item->RoomNumber, 0);
			}
			else if (coll->Setup.EnableObjectPush)
			{
				// TODO: Test. Might have player walking through objects in edge cases.
				// Avoid interfering with player object interactions.
				/*if (isPlayer && !GetLaraInfo(*item).Control.IsMoving)
					continue;*/

				ItemPushStatic(item, staticObject, coll);
			}
			else
			{
				continue;
			}
		}
	}

	// TODO: Rewrite player spasm effect.
	if (isPlayer)
	{
		auto& player = GetLaraInfo(*item);
		if (player.HitDirection == -1)
			player.HitFrame = 0;
	}
}

void AIPickupCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	auto* item = &g_Level.Items[itemNumber];

	if (item->ObjectNumber == ID_SHOOT_SWITCH1 && !(item->MeshBits & 1))
		item->Status = ITEM_INVISIBLE;
}

void ObjectCollision(const short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	auto* item = &g_Level.Items[itemNumber];

	if (TestBoundsCollide(item, laraItem, coll->Setup.Radius))
	{
		if (HandleItemSphereCollision(*item, *laraItem))
		{
			if (coll->Setup.EnableObjectPush)
				ItemPushItem(item, laraItem, coll, false, 1);
		}
	}
}

void CreatureCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	auto* item = &g_Level.Items[itemNumber];

	if (!TestBoundsCollide(item, laraItem, coll->Setup.Radius))
		return;

	if (!HandleItemSphereCollision(*item, *laraItem))
		return;

	bool doPlayerCollision = laraItem->IsLara();
	bool waterPlayerCollision = doPlayerCollision && GetLaraInfo(laraItem)->Control.WaterStatus >= WaterStatus::TreadWater;

	if (waterPlayerCollision || coll->Setup.EnableObjectPush)
	{
		ItemPushItem(item, laraItem, coll, coll->Setup.EnableSpasm, 0);
	}
	else if (doPlayerCollision && coll->Setup.EnableSpasm)
	{
		// TODO: Rewrite player spasm effect.
		return;

		const auto& bounds = GameBoundingBox(item);
		if (bounds.GetHeight() > CLICK(1))
		{
			auto* lara = GetLaraInfo(laraItem);

			lara->HitDirection = NORTH;
			lara->HitFrame = 0;
		}
	}
}

void TrapCollision(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
{
	auto& item = g_Level.Items[itemNumber];

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
}
