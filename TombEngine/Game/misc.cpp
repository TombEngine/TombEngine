#include "framework.h"
#include "Game/misc.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/Point.h"
#include "Game/Lara/lara.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Utils;

CreatureInfo* GetCreatureInfo(ItemInfo* item)
{
	return (CreatureInfo*)item->Data;
}

bool IsNextSectorValid(const ItemInfo& item, const Vector3& dir, float dist, bool canFloat)
{
	auto projectedPos = Geometry::TranslatePoint(item.Pose.Position, dir, dist);
	auto pointColl = GetPointCollision(item.Pose.Position, item.RoomNumber, dir, dist);
	int height = GameBoundingBox(&item).GetHeight();

	// TODO: Use floor normal directly.
	auto floorTilt = GetSurfaceTilt(pointColl.GetFloorNormal(), true);

	// Test for wall.
	if (pointColl.GetSector().IsWall(projectedPos.x, projectedPos.z))
		return false;

	// Test for slippery slope.
	if (pointColl.IsSteepFloor())
		return false;

	// Flat floor.
	if ((abs(floorTilt.x) == 0 && abs(floorTilt.y) == 0))
	{
		// Test for step.
		int relFloorHeight = abs(pointColl.GetFloorHeight() - item.Pose.Position.y);
		if (relFloorHeight >= CLICK(1) && item.Pose.Position.y >= pointColl.GetFloorHeight() && canFloat)
		{
			return false;
		}		
		else if (relFloorHeight >= CLICK(1) && !canFloat)
		{
			return false;
		}
	}
	// Sloped floor.
	else
	{
		// Half block.
		int relFloorHeight = abs(pointColl.GetFloorHeight() - item.Pose.Position.y);
		if (relFloorHeight > CLICK(1) && canFloat)
		{
			return false;
		}
		else if (relFloorHeight > CLICK(2) && !canFloat)
		{
			return false;
		}

		short slopeAngle = ANGLE(0.0f);
		if (floorTilt.x > 0)
		{
			slopeAngle = ANGLE(-90.0f);
		}
		else if (floorTilt.x < 0)
		{
			slopeAngle = ANGLE(90.0f);
		}

		if (floorTilt.y > 0 && floorTilt.y > abs(floorTilt.x))
		{
			slopeAngle = ANGLE(180.0f);
		}
		else if (floorTilt.y < 0 && -floorTilt.y > abs(floorTilt.x))
		{
			slopeAngle = ANGLE(0.0f);
		}

		short dirAngle = phd_atan(dir.z, dir.x);
		short alignAngle = Geometry::GetShortestAngle(slopeAngle, dirAngle);

		// Test if slope aspect is aligned with direction.
		if (alignAngle != 0 && alignAngle != ANGLE(180.0f))
			return false;
	}

	// Check for diagonal split.
	if (pointColl.IsDiagonalFloorStep())
		return false;

	// Test ceiling height.
	int relCeilHeight = abs(pointColl.GetCeilingHeight() - pointColl.GetFloorHeight());

	if (canFloat)
	{
		if (relCeilHeight <= height)
			return false;
	}
	else
	{
		if (relCeilHeight < BLOCK(1))
			return false;
	}

	// Check for inaccessible sector.
	if (pointColl.GetSector().PathfindingBoxID == NO_VALUE)
		return false;

	// Check for blocked grey box.
	if (g_Level.PathfindingBoxes[pointColl.GetSector().PathfindingBoxID].flags & BLOCKABLE)
	{
		if (g_Level.PathfindingBoxes[pointColl.GetSector().PathfindingBoxID].flags & BLOCKED)
			return false;
	}

	// Check for stopper flag.
	if (pointColl.GetSector().Stopper)
		return false;

	return true;
}
