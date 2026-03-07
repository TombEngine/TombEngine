#include "framework.h"
#include "Game/collision/collide_room.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/items.h"
#include "Game/room.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/trutils.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Collision::Room;
using namespace TEN::Math;
using namespace TEN::Utils;

void ShiftItem(ItemInfo* item, CollisionInfo* coll)
{
	item->Pose.Position += coll->Shift.Position;
	item->Pose.Orientation += coll->Shift.Orientation;
	coll->Shift = Vector3i::Zero;
}

void SnapEntityToGrid(ItemInfo& item, const CollisionInfo& coll)
{
	int quadrant = GetQuadrant(item.Pose.Orientation.y);
	switch (quadrant)
	{
	default:
	case NORTH:
		item.Pose.Position.z = (item.Pose.Position.z | WALL_MASK) - coll.Setup.Radius;
		break;

	case EAST:
		item.Pose.Position.x = (item.Pose.Position.x | WALL_MASK) - coll.Setup.Radius;
		break;

	case SOUTH:
		item.Pose.Position.z = (item.Pose.Position.z & ~WALL_MASK) + coll.Setup.Radius;
		break;

	case WEST:
		item.Pose.Position.x = (item.Pose.Position.x & ~WALL_MASK) + coll.Setup.Radius;
		break;
	}
}

int FindGridShift(int x, int z)
{
	if ((x / BLOCK(1)) == (z / BLOCK(1)))
		return 0;

	if ((z / BLOCK(1)) <= (x / BLOCK(1)))
	{
		return (-1 - (x & WALL_MASK));
	}
	else
	{
		return ((BLOCK(1) + 1) - (x & WALL_MASK));
	}
}

// Test if the axis-aligned bounding box collides with geometry at all.
bool TestItemRoomCollisionAABB(ItemInfo* item)
{
	const auto& bounds = GetClosestKeyframe(*item).BoundingBox;
	auto box = bounds + item->Pose;
	short maxY = std::min(box.Y1, box.Y2);
	short minY = std::max(box.Y1, box.Y2);

	auto test = [item](int x, int y, int z, bool floor)
	{
		auto pointColl = GetPointCollision(Vector3i(x, y, z), item->RoomNumber);
		
		if (floor)
		{
			return (y > pointColl.GetFloorHeight());
		}
		else
		{
			return (y < pointColl.GetCeilingHeight());
		}
	};

	bool hasCollided =
		test(box.X1, minY, box.Z1, true) ||
		test(box.X2, minY, box.Z1, true) ||
		test(box.X1, minY, box.Z2, true) ||
		test(box.X2, minY, box.Z2, true) ||
		test(box.X1, maxY, box.Z1, false) ||
		test(box.X2, maxY, box.Z1, false) ||
		test(box.X1, maxY, box.Z2, false) ||
		test(box.X2, maxY, box.Z2, false);

	return hasCollided;
}

static CollisionPositionData GetCollisionPosition(PointCollisionData& pointColl)
{
	auto collPos = CollisionPositionData{};
	collPos.Floor = pointColl.GetFloorHeight();
	collPos.Ceiling = pointColl.GetCeilingHeight();
	collPos.Bridge = pointColl.GetFloorBridgeItemNumber();
	collPos.SplitAngle = pointColl.GetBottomSector().FloorSurface.SplitAngle;
	collPos.FloorSlope = pointColl.IsSteepFloor();
	collPos.CeilingSlope = pointColl.IsSteepCeiling();
	collPos.DiagonalStep = pointColl.IsDiagonalFloorStep();

	return collPos;
}

static void SetSectorAttribs(CollisionPositionData& sectorAttribs, const CollisionSetupData& collSetup, PointCollisionData& pointColl,
							 const Vector3i& probePos, int realRoomNumber)
{
	constexpr auto ASPECT_ANGLE_DELTA_MAX = ANGLE(90.0f);

	auto floorNormal = pointColl.GetFloorNormal();
	short aspectAngle = Geometry::GetSurfaceAspectAngle(floorNormal);
	short aspectAngleDelta = Geometry::GetShortestAngle(collSetup.ForwardAngle, aspectAngle);

	if (collSetup.BlockFloorSlopeUp &&
		sectorAttribs.FloorSlope &&
		sectorAttribs.Floor <= STEPUP_HEIGHT &&
		sectorAttribs.Floor >= -STEPUP_HEIGHT &&
		abs(aspectAngleDelta) >= ASPECT_ANGLE_DELTA_MAX)
	{
		sectorAttribs.Floor = MAX_HEIGHT;
	}
	else if (collSetup.BlockFloorSlopeDown &&
		sectorAttribs.FloorSlope &&
		sectorAttribs.Floor <= STEPUP_HEIGHT &&
		sectorAttribs.Floor >= -STEPUP_HEIGHT &&
		abs(aspectAngleDelta) <= ASPECT_ANGLE_DELTA_MAX)
	{
		sectorAttribs.Floor = MAX_HEIGHT;
	}
	else if (collSetup.BlockCeilingSlope &&
		sectorAttribs.CeilingSlope)
	{
		sectorAttribs.Floor = MAX_HEIGHT;
	}
	else if (collSetup.BlockDeathFloorDown &&
		sectorAttribs.Floor >= CLICK(0.5f) &&
		pointColl.GetBottomSector().Flags.Death)
	{
		sectorAttribs.Floor = MAX_HEIGHT;
	}
	else if (collSetup.BlockMonkeySwingEdge)
	{
		auto pointColl = GetPointCollision(probePos, realRoomNumber, Vector3::UnitY, collSetup.Height);
		if (!pointColl.GetBottomSector().Flags.Monkeyswing)
			sectorAttribs.Floor = MAX_HEIGHT;
	}
}

static void HandleDiagonalShift(ItemInfo& item, CollisionInfo& coll, const Vector3i& pos, CardinalDirection quadrant, bool isRightShift)
{
	// Calculate angles.
	int sign = isRightShift ? 1 : -1;
	short perpSplitAngle = (ANGLE(90.0f) * quadrant) + (ANGLE(45.0f) * sign);
	short deltaAngle = abs(Geometry::GetShortestAngle(coll.Setup.ForwardAngle, perpSplitAngle));

	// HACK: Force slight push left to avoid getting stuck.
	float alpha = 1.0f - ((float)deltaAngle / (float)ANGLE(90.0f));
	if (alpha >= 0.5f)
		item.Pose.Translate(perpSplitAngle, item.Animation.Velocity.z * alpha);

	// Set shift.
	coll.Shift.Position.x += coll.Setup.PrevPosition.x - pos.x;
	coll.Shift.Position.z += coll.Setup.PrevPosition.z - pos.z;
};

void GetCollisionInfo(CollisionInfo* coll, ItemInfo* item, const Vector3i& offset, bool resetRoom)
{
	// Player collision has several more precise checks for bridge collisions.
	// Therefore, we should differentiate these code paths.
	bool doPlayerCollision = item->IsLara();

	// Reset collision parameters.
	coll->CollisionType = CollisionType::None;
	coll->Shift = Pose::Zero;

	// Offset base probe position by provided offset, if any.
	auto entityPos = item->Pose.Position + offset;

	// Specify base probe position, with Y position being bounds top side.
	auto probePos = Vector3i(entityPos.x, entityPos.y - coll->Setup.Height, entityPos.z);

	// Declare side probe offsets.
	int xFront, zFront, xRight, zRight, xLeft, zLeft;

	// Get nearest 90-degree snapped angle (quadrant).
	int quadrant = GetQuadrant(coll->Setup.ForwardAngle);

	// Get side probe offsets depending on quadrant.
	// If unconstrained mode is specified, don't use quadrant.
	switch ((coll->Setup.Mode == CollisionProbeMode::Quadrants) ? quadrant : -1)
	{
	case NORTH:
		xFront =  phd_sin(coll->Setup.ForwardAngle) * coll->Setup.Radius;
		zFront =  coll->Setup.Radius;
		xLeft  = -coll->Setup.Radius;
		zLeft  =  coll->Setup.Radius;
		xRight =  coll->Setup.Radius;
		zRight =  coll->Setup.Radius;
		break;

	case EAST:
		xFront =  coll->Setup.Radius;
		zFront =  phd_cos(coll->Setup.ForwardAngle) * coll->Setup.Radius;
		xLeft  =  coll->Setup.Radius;
		zLeft  =  coll->Setup.Radius;
		xRight =  coll->Setup.Radius;
		zRight = -coll->Setup.Radius;
		break;

	case SOUTH:
		xFront =  phd_sin(coll->Setup.ForwardAngle) * coll->Setup.Radius;
		zFront = -coll->Setup.Radius;
		xLeft  =  coll->Setup.Radius;
		zLeft  = -coll->Setup.Radius;
		xRight = -coll->Setup.Radius;
		zRight = -coll->Setup.Radius;
		break;

	case WEST:
		xFront = -coll->Setup.Radius;
		zFront =  phd_cos(coll->Setup.ForwardAngle) * coll->Setup.Radius;
		xLeft  = -coll->Setup.Radius;
		zLeft  = -coll->Setup.Radius;
		xRight = -coll->Setup.Radius;
		zRight =  coll->Setup.Radius;
		break;

	// No valid quadrant; get true probe offsets from object rotation.
	default: 
		xFront = phd_sin(coll->Setup.ForwardAngle) * coll->Setup.Radius;
		zFront = phd_cos(coll->Setup.ForwardAngle) * coll->Setup.Radius;
		xLeft  = (xFront * (coll->Setup.Mode == CollisionProbeMode::FreeForward ? 0.5f : 1.0f)) + phd_sin(coll->Setup.ForwardAngle - ANGLE(90.0f)) * coll->Setup.Radius;
		zLeft  = (zFront * (coll->Setup.Mode == CollisionProbeMode::FreeForward ? 0.5f : 1.0f)) + phd_cos(coll->Setup.ForwardAngle - ANGLE(90.0f)) * coll->Setup.Radius;
		xRight = (xFront * (coll->Setup.Mode == CollisionProbeMode::FreeForward ? 0.5f : 1.0f)) + phd_sin(coll->Setup.ForwardAngle + ANGLE(90.0f)) * coll->Setup.Radius;
		zRight = (zFront * (coll->Setup.Mode == CollisionProbeMode::FreeForward ? 0.5f : 1.0f)) + phd_cos(coll->Setup.ForwardAngle + ANGLE(90.0f)) * coll->Setup.Radius;
		break;
	}

	// Define generic variables used for later object-specific position test shifts.
	auto tfLocation = RoomVector();
	auto tcLocation = RoomVector();
	auto lrfLocation = RoomVector();
	auto lrcLocation = RoomVector();
	int height = 0;
	int ceiling = 0;

	// Parameter definition ends here, now process to actual collision tests...

	// HACK: when using SetPosition animcommand, item->RoomNumber does not immediately
	// update, but only at the end of control loop. This may cause bugs when Lara is
	// climbing or vaulting ledges under slopes. Using Location->RoomNumber solves
	// these bugs, as it is updated immediately. But since Location->RoomNumber is ONLY
	// updated for Lara, we can't use it for all objects for now. In future, we should
	// either update Location field for all objects or use this value as it is now.

	int realRoomNumber = doPlayerCollision ? item->Location.RoomNumber : item->RoomNumber;
	
	// TEST 1: TILT AND NEAREST LEDGE CALCULATION

	auto pointColl = GetPointCollision(Vector3i(probePos.x, item->Pose.Position.y, probePos.z), realRoomNumber);

	coll->FloorNormal = pointColl.GetFloorNormal();
	coll->CeilingNormal = pointColl.GetCeilingNormal();
	coll->FloorTilt = GetSurfaceTilt(pointColl.GetFloorNormal(), true).ToVector2();
	coll->CeilingTilt = GetSurfaceTilt(pointColl.GetCeilingNormal(), false).ToVector2();
	//coll->NearestLedgeAngle = GetNearestLedgeAngle(item, coll, coll->NearestLedgeDistance);

	// Debug angle and distance
	// PrintDebugMessage("Nearest angle: %d", coll->NearestLedgeAngle);
	// PrintDebugMessage("Nearest dist:  %f", coll->NearestLedgeDistance);
	
	// TEST 2: CENTERPOINT PROBE

	pointColl = GetPointCollision(probePos, realRoomNumber);
	int topRoomNumber = pointColl.GetRoomNumber(); // Keep top room number as we need it to re-probe from origin room.
	
	if (doPlayerCollision)
	{
		tfLocation = GetRoomVector(item->Location, probePos);
		height = GetSurfaceHeight(tfLocation, probePos.x, probePos.z, true).value_or(NO_HEIGHT);

		tcLocation = GetRoomVector(item->Location, Vector3i(probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z));
		ceiling = GetSurfaceHeight(tcLocation, probePos.x, probePos.z, false).value_or(NO_HEIGHT);
	}
	else
	{
		height = pointColl.GetFloorHeight();
		ceiling = GetCeiling(&pointColl.GetSector(), probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z);
	}

	if (height != NO_HEIGHT)
		height -= (doPlayerCollision ? entityPos.y : probePos.y);

	if (ceiling != NO_HEIGHT)
		ceiling -= probePos.y;

	coll->Middle = GetCollisionPosition(pointColl);
	coll->Middle.Floor = height;
	coll->Middle.Ceiling = ceiling;

	// Additionally calculate bridge shifts if present.
	CollideBridgeItems(*item, *coll, pointColl);

	// TEST 3: FRONTAL PROBE

	probePos.x = entityPos.x + xFront;
	probePos.z = entityPos.z + zFront;

	DrawDebugSphere(probePos.ToVector3(), 32, Vector4(1, 0, 0, 1), RendererDebugPage::CollisionStats);

	pointColl = GetPointCollision(probePos, topRoomNumber);

	if (doPlayerCollision)
	{
		if (resetRoom)
		{
			tfLocation = item->Location;
			tcLocation = item->Location;
			topRoomNumber = realRoomNumber;
		}

		tfLocation = GetRoomVector(tfLocation, probePos);
		height = GetSurfaceHeight(tfLocation, probePos.x, probePos.z, true).value_or(NO_HEIGHT);

		tcLocation = GetRoomVector(tcLocation, Vector3i(probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z));
		ceiling = GetSurfaceHeight(tcLocation, probePos.x, probePos.z, false).value_or(NO_HEIGHT);
	}
	else
	{
		height = pointColl.GetFloorHeight();
		ceiling = GetCeiling(&pointColl.GetSector(), probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z);
	}

	if (height != NO_HEIGHT)
		height -= (doPlayerCollision ? entityPos.y : probePos.y);

	if (ceiling != NO_HEIGHT)
		ceiling -= probePos.y;

	coll->Front = GetCollisionPosition(pointColl);
	coll->Front.Floor = height;
	coll->Front.Ceiling = ceiling;

	if (doPlayerCollision)
	{
		tfLocation = GetRoomVector(tfLocation, Vector3i(probePos.x + xFront, probePos.y, probePos.z + zFront));
		height = GetSurfaceHeight(tfLocation, probePos.x + xFront, probePos.z + zFront, true).value_or(NO_HEIGHT);
	}
	else
	{
		height = GetPointCollision(Vector3i(probePos.x + xFront, probePos.y, probePos.z + zFront), topRoomNumber).GetFloorHeight();
	}
	
	if (height != NO_HEIGHT)
		height -= (doPlayerCollision ? entityPos.y : probePos.y);

	SetSectorAttribs(coll->Front, coll->Setup, pointColl, probePos, realRoomNumber);

	// TEST 4: MIDDLE-LEFT PROBE

	probePos.x = entityPos.x + xLeft;
	probePos.z = entityPos.z + zLeft;

	DrawDebugSphere(probePos.ToVector3(), 32, Vector4(0, 0, 1, 1), RendererDebugPage::CollisionStats);

	pointColl = GetPointCollision(probePos, item->RoomNumber);

	if (doPlayerCollision)
	{
		lrfLocation = GetRoomVector(item->Location, probePos);
		height = GetSurfaceHeight(lrfLocation, probePos.x, probePos.z, true).value_or(NO_HEIGHT);

		lrcLocation = GetRoomVector(item->Location, Vector3i(probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z));
		ceiling = GetSurfaceHeight(lrcLocation, probePos.x, probePos.z, false).value_or(NO_HEIGHT);
	}
	else
	{
		height = pointColl.GetFloorHeight();
		ceiling = GetCeiling(&pointColl.GetSector(), probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z);
	}

	if (height != NO_HEIGHT)
		height -= (doPlayerCollision ? entityPos.y : probePos.y);

	if (ceiling != NO_HEIGHT)
		ceiling -= probePos.y;

	coll->MiddleLeft = GetCollisionPosition(pointColl);
	coll->MiddleLeft.Floor = height;
	coll->MiddleLeft.Ceiling = ceiling;

	SetSectorAttribs(coll->MiddleLeft, coll->Setup, pointColl, probePos, realRoomNumber);

	// TEST 5: FRONT-LEFT PROBE

	// Use plain X/Z values.
	pointColl = GetPointCollision(probePos, topRoomNumber);

	if (doPlayerCollision)
	{
		tfLocation = GetRoomVector(tfLocation, probePos);
		height = GetSurfaceHeight(tfLocation, probePos.x, probePos.z, true).value_or(NO_HEIGHT);

		tcLocation = GetRoomVector(tcLocation, Vector3i(probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z));
		ceiling = GetSurfaceHeight(tcLocation, probePos.x, probePos.z, false).value_or(NO_HEIGHT);
	}
	else
	{
		height = pointColl.GetFloorHeight();
		ceiling = GetCeiling(&pointColl.GetSector(), probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z);
	}

	if (height != NO_HEIGHT)
		height -= (doPlayerCollision ? entityPos.y : probePos.y);

	if (ceiling != NO_HEIGHT)
		ceiling -= probePos.y;

	coll->FrontLeft = GetCollisionPosition(pointColl);
	coll->FrontLeft.Floor = height;
	coll->FrontLeft.Ceiling = ceiling;

	SetSectorAttribs(coll->FrontLeft, coll->Setup, pointColl, probePos, realRoomNumber);

	// TEST 6: MIDDLE-RIGHT PROBE

	probePos.x = entityPos.x + xRight;
	probePos.z = entityPos.z + zRight;

	DrawDebugSphere(probePos.ToVector3(), 32, Vector4(0, 1, 0, 1), RendererDebugPage::CollisionStats);

	pointColl = GetPointCollision(probePos, item->RoomNumber);

	if (doPlayerCollision)
	{
		lrfLocation = GetRoomVector(item->Location, probePos);
		height = GetSurfaceHeight(lrfLocation, probePos.x, probePos.z, true).value_or(NO_HEIGHT);

		lrcLocation = GetRoomVector(item->Location, Vector3i(probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z));
		ceiling = GetSurfaceHeight(lrcLocation, probePos.x, probePos.z, false).value_or(NO_HEIGHT);
	}
	else
	{
		height = pointColl.GetFloorHeight();
		ceiling = GetCeiling(&pointColl.GetSector(), probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z);
	}

	if (height != NO_HEIGHT)
		height -= (doPlayerCollision ? entityPos.y : probePos.y);

	if (ceiling != NO_HEIGHT)
		ceiling -= probePos.y;

	coll->MiddleRight = GetCollisionPosition(pointColl);
	coll->MiddleRight.Floor = height;
	coll->MiddleRight.Ceiling = ceiling;

	SetSectorAttribs(coll->MiddleRight, coll->Setup, pointColl, probePos, realRoomNumber);

	// TEST 7: FRONT-RIGHT PROBE

	pointColl = GetPointCollision(probePos, topRoomNumber);

	if (doPlayerCollision)
	{
		tfLocation = GetRoomVector(tfLocation, probePos);
		height = GetSurfaceHeight(tfLocation, probePos.x, probePos.z, true).value_or(NO_HEIGHT);

		tcLocation = GetRoomVector(tcLocation, Vector3i(probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z));
		ceiling = GetSurfaceHeight(tcLocation, probePos.x, probePos.z, false).value_or(NO_HEIGHT);
	}
	else
	{
		height = pointColl.GetFloorHeight();
		ceiling = GetCeiling(&pointColl.GetSector(), probePos.x, probePos.y - item->Animation.Velocity.y, probePos.z);
	}

	if (height != NO_HEIGHT)
		height -= (doPlayerCollision ? entityPos.y : probePos.y);

	if (ceiling != NO_HEIGHT)
		ceiling -= probePos.y;

	coll->FrontRight = GetCollisionPosition(pointColl);
	coll->FrontRight.Floor = height;
	coll->FrontRight.Ceiling = ceiling;

	SetSectorAttribs(coll->FrontRight, coll->Setup, pointColl, probePos, realRoomNumber);

	// TEST 8: SOLID STATIC MESHES

	CollideSolidStatics(item, coll);

	// Collision tests now end.
	// Get to calculation of collision side and shifts.

	if (coll->Middle.Floor == NO_HEIGHT)
	{
		coll->Shift.Position += coll->Setup.PrevPosition - entityPos;
		coll->CollisionType = CollisionType::Front;
		return;
	}

	if (coll->Middle.Floor - coll->Middle.Ceiling <= 0)
	{
		coll->Shift.Position += coll->Setup.PrevPosition - entityPos;
		coll->CollisionType = CollisionType::Clamp;
		return;
	}

	if (coll->Middle.Ceiling >= 0)
	{
		coll->Shift.Position.y += coll->Middle.Ceiling;
		coll->CollisionType = CollisionType::Top;
	}

	// Shift away from front wall.
	if (coll->Front.Floor > coll->Setup.LowerFloorBound ||
		coll->Front.Floor < coll->Setup.UpperFloorBound ||
		coll->Front.Ceiling > coll->Setup.LowerCeilingBound ||
		coll->Front.Ceiling < coll->Setup.UpperCeilingBound ||
		coll->Front.Floor - coll->Front.Ceiling <= 0)
	{
		if (coll->Front.HasDiagonalSplit())
		{
			coll->Shift.Position.x += coll->Setup.PrevPosition.x - entityPos.x;
			coll->Shift.Position.z += coll->Setup.PrevPosition.z - entityPos.z;
		}
		else
		{
			switch (quadrant)
			{
			case NORTH:
			case SOUTH:
				coll->Shift.Position.x += coll->Setup.PrevPosition.x - entityPos.x;
				coll->Shift.Position.z += FindGridShift(entityPos.z + zFront, entityPos.z);
				break;

			case EAST:
			case WEST:
				coll->Shift.Position.x += FindGridShift(entityPos.x + xFront, entityPos.x);
				coll->Shift.Position.z += coll->Setup.PrevPosition.z - entityPos.z;
				break;
			}
		}

		coll->CollisionType = (coll->CollisionType == CollisionType::Top) ? CollisionType::TopFront : CollisionType::Front;
		return;
	}

	if (coll->Front.Ceiling > coll->Setup.LowerCeilingBound ||
		coll->Front.Ceiling < coll->Setup.UpperCeilingBound)
	{
		coll->Shift.Position += coll->Setup.PrevPosition - entityPos;
		coll->CollisionType = CollisionType::TopFront;
		return;
	}

	// Shift away from left wall.
	if (coll->MiddleLeft.Floor > coll->Setup.LowerFloorBound ||
		coll->MiddleLeft.Floor < coll->Setup.UpperFloorBound ||
		coll->MiddleLeft.Ceiling > coll->Setup.LowerCeilingBound ||
		coll->MiddleLeft.Ceiling < coll->Setup.UpperCeilingBound ||
		coll->MiddleLeft.Floor - coll->MiddleLeft.Ceiling <= 0)
	{
		if (coll->TriangleAtLeft() && !coll->MiddleLeft.FloorSlope)
		{
			HandleDiagonalShift(*item, *coll, entityPos, (CardinalDirection)quadrant, true);
		}
		else
		{
			switch (quadrant)
			{
			case NORTH:
			case SOUTH:
				coll->Shift.Position.x += FindGridShift(entityPos.x + xLeft, entityPos.x + xFront);
				break;

			case EAST:
			case WEST:
				coll->Shift.Position.z += FindGridShift(entityPos.z + zLeft, entityPos.z + zFront);
				break;
			}
		}

		if (coll->DiagonalStepAtLeft())
		{
			// NOTE: Different from quadrant.
			int quarter = (unsigned short)coll->Setup.ForwardAngle / ANGLE(90.0f);
			quarter %= 2;

			if (coll->MiddleLeft.HasFlippedDiagonalSplit())
			{
				if (quarter)
					coll->CollisionType = CollisionType::Left;
			}
			else
			{
				if (!quarter)
					coll->CollisionType = CollisionType::Left;
			}
		}
		else
		{
			coll->CollisionType = CollisionType::Left;
		}

		return;
	}

	// Shift away from right wall.
	if (coll->MiddleRight.Floor > coll->Setup.LowerFloorBound ||
		coll->MiddleRight.Floor < coll->Setup.UpperFloorBound ||
		coll->MiddleRight.Ceiling > coll->Setup.LowerCeilingBound ||
		coll->MiddleRight.Ceiling < coll->Setup.UpperCeilingBound ||
		coll->MiddleRight.Floor - coll->MiddleRight.Ceiling <= 0)
	{
		if (coll->TriangleAtRight() && !coll->MiddleRight.FloorSlope)
		{
			HandleDiagonalShift(*item, *coll, entityPos, (CardinalDirection)quadrant, false);
		}
		else
		{
			switch (quadrant)
			{
			case NORTH:
			case SOUTH:
				coll->Shift.Position.x += FindGridShift(entityPos.x + xRight, entityPos.x + xFront);
				break;

			case EAST:
			case WEST:
				coll->Shift.Position.z += FindGridShift(entityPos.z + zRight, entityPos.z + zFront);
				break;
			}
		}

		if (coll->DiagonalStepAtRight())
		{
			int quarter = unsigned short(coll->Setup.ForwardAngle) / ANGLE(90.0f); // NOTE: Different from quadrant!
			quarter %= 2;

			if (coll->MiddleRight.HasFlippedDiagonalSplit())
			{
				if (quarter)
					coll->CollisionType = CollisionType::Right;
			}
			else
			{
				if (!quarter)
					coll->CollisionType = CollisionType::Right;
			}
		}
		else
		{
			coll->CollisionType = CollisionType::Right;
		}

		return;
	}
}

void GetCollisionInfo(CollisionInfo* coll, ItemInfo* item, bool resetRoom)
{
	GetCollisionInfo(coll, item, Vector3i::Zero, resetRoom);
}

void AlignEntityToSurface(ItemInfo* item, const Vector2& ellipse, float alpha, short constraintAngle)
{
	// Reduce ellipse axis lengths for stability.
	auto reducedEllipse = ellipse * 0.75f;

	// Probe heights at points around entity.
	int frontHeight = GetPointCollision(*item, item->Pose.Orientation.y, reducedEllipse.y).GetFloorHeight();
	int backHeight	= GetPointCollision(*item, item->Pose.Orientation.y + ANGLE(180.0f), reducedEllipse.y).GetFloorHeight();
	int leftHeight	= GetPointCollision(*item, item->Pose.Orientation.y - ANGLE(90.0f), reducedEllipse.x).GetFloorHeight();
	int rightHeight = GetPointCollision(*item, item->Pose.Orientation.y + ANGLE(90.0f), reducedEllipse.x).GetFloorHeight();

	// Calculate height deltas.
	int forwardHeightDelta = backHeight - frontHeight;
	int lateralHeightDelta = rightHeight - leftHeight;

	// Calculate extra rotation required.
	auto extraRot = EulerAngles(
		FROM_RAD(atan2(forwardHeightDelta, ellipse.y * 2)),
		0,
		FROM_RAD(atan2(lateralHeightDelta, ellipse.x * 2))) -
		EulerAngles(item->Pose.Orientation.x, 0, item->Pose.Orientation.z);

	// Rotate X axis.
	if (abs(forwardHeightDelta) <= STEPUP_HEIGHT)
	{
		if (abs(extraRot.x) <= constraintAngle)
			item->Pose.Orientation.x += extraRot.x * alpha;
	}

	// Rotate Z axis.
	if (abs(lateralHeightDelta) <= STEPUP_HEIGHT)
	{
		if (abs(extraRot.z) <= constraintAngle)
			item->Pose.Orientation.z += extraRot.z * alpha;
	}
}

int GetQuadrant(short angle)
{
	return (unsigned short(angle + ANGLE(45.0f)) / ANGLE(90.0f));
}

FloorInfo* GetFloor(int x, int y, int z, short* roomNumber)
{
	auto location = GetRoomVector(RoomVector(*roomNumber, y), Vector3i(x, y, z));
	*roomNumber = location.RoomNumber;
	return &GetFloor(*roomNumber, x, z);
}

int GetFloorHeight(FloorInfo* floor, int x, int y, int z)
{
	return GetSurfaceHeight(RoomVector(floor->RoomNumber, y), x, z, true).value_or(NO_HEIGHT);
}

int GetCeiling(FloorInfo* floor, int x, int y, int z)
{
	return GetSurfaceHeight(RoomVector(floor->RoomNumber, y), x, z, false).value_or(NO_HEIGHT);
}

int GetDistanceToFloor(int itemNumber, bool precise)
{
	const auto& item = g_Level.Items[itemNumber];

	auto pointColl = GetPointCollision(item);

	// HACK: Remove item from bridge objects temporarily.
	pointColl.GetSector().RemoveBridge(itemNumber);
	int height = GetFloorHeight(&pointColl.GetSector(), item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z);
	pointColl.GetSector().AddBridge(itemNumber);

	auto bounds = GameBoundingBox(&item);
	int minHeight = precise ? bounds.Y2 : 0;

	return (minHeight + item.Pose.Position.y - height);
}

bool TestEnvironment(RoomEnvFlags environmentType, int x, int y, int z, int roomNumber)
{
	return TestEnvironment(environmentType, GetPointCollision(Vector3i(x, y, z), roomNumber).GetRoomNumber());
}

bool TestEnvironment(RoomEnvFlags environmentType, const Vector3i& pos, int roomNumber)
{
	return TestEnvironment(environmentType, GetPointCollision(pos, roomNumber).GetRoomNumber());
}

bool TestEnvironment(RoomEnvFlags environmentType, const ItemInfo* item)
{
	return TestEnvironment(environmentType, item->RoomNumber);
}

bool TestEnvironment(RoomEnvFlags environmentType, int roomNumber)
{
	return TestEnvironment(environmentType, &g_Level.Rooms[roomNumber]);
}

bool TestEnvironment(RoomEnvFlags environmentType, const RoomData* room)
{
	return TestEnvironmentFlags(environmentType, room->flags);
}

bool TestEnvironmentFlags(RoomEnvFlags environmentType, int flags)
{
	return ((flags & environmentType) == environmentType);
}
