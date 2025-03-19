#include "framework.h"
#include "Game/Lara/lara_tests.h"

#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/control/los.h"
#include "Game/items.h"
#include "Game/Lara/PlayerContext.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_climb.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_monkey.h"
#include "Math/Math.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Entities::Player;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Utils;

// -----------------------------
// TEST FUNCTIONS
// For State Control & Collision
// -----------------------------

// Test if a ledge in front of item is valid to climb.
bool TestValidLedge(ItemInfo* item, CollisionInfo* coll, bool ignoreHeadroom, bool heightLimit)
{
	// Determine probe base left/right points
	int xl = phd_sin(coll->NearestLedgeAngle - ANGLE(90.0f)) * coll->Setup.Radius;
	int zl = phd_cos(coll->NearestLedgeAngle - ANGLE(90.0f)) * coll->Setup.Radius;
	int xr = phd_sin(coll->NearestLedgeAngle + ANGLE(90.0f)) * coll->Setup.Radius;
	int zr = phd_cos(coll->NearestLedgeAngle + ANGLE(90.0f)) * coll->Setup.Radius;

	// Determine probe top point
	int y = item->Pose.Position.y - coll->Setup.Height;

	// Get frontal collision data
	auto frontLeft  = GetPointCollision(Vector3i(item->Pose.Position.x + xl, y, item->Pose.Position.z + zl), GetRoomVector(item->Location, Vector3i(item->Pose.Position.x, y, item->Pose.Position.z)).RoomNumber);
	auto frontRight = GetPointCollision(Vector3i(item->Pose.Position.x + xr, y, item->Pose.Position.z + zr), GetRoomVector(item->Location, Vector3i(item->Pose.Position.x, y, item->Pose.Position.z)).RoomNumber);

	// If any of the frontal collision results intersects item bounds, return false, because there is material intersection.
	// This check helps to filter out cases when Lara is formally facing corner but ledge check returns true because probe distance is fixed.
	if (frontLeft.GetFloorHeight() < (item->Pose.Position.y - CLICK(0.5f)) || frontRight.GetFloorHeight() < (item->Pose.Position.y - CLICK(0.5f)))
		return false;
	if (frontLeft.GetCeilingHeight() >(item->Pose.Position.y - coll->Setup.Height) || frontRight.GetCeilingHeight() > (item->Pose.Position.y - coll->Setup.Height))
		return false;

	//DrawDebugSphere(Vector3(item->pos.Position.x + xl, left, item->pos.Position.z + zl), 64, Vector4::One, RendererDebugPage::CollisionStats);
	//DrawDebugSphere(Vector3(item->pos.Position.x + xr, right, item->pos.Position.z + zr), 64, Vector4::One, RendererDebugPage::CollisionStats);
	
	// Determine ledge probe embed offset.
	// We use 0.2f radius extents here for two purposes. First - we can't guarantee that shifts weren't already applied
	// and misfire may occur. Second - it guarantees that Lara won't land on a very thin edge of diagonal geometry.
	int xf = phd_sin(coll->NearestLedgeAngle) * (coll->Setup.Radius * 1.2f);
	int zf = phd_cos(coll->NearestLedgeAngle) * (coll->Setup.Radius * 1.2f);

	// Get floor heights at both points
	auto left = GetPointCollision(Vector3i(item->Pose.Position.x + xf + xl, y, item->Pose.Position.z + zf + zl), GetRoomVector(item->Location, Vector3i(item->Pose.Position.x, y, item->Pose.Position.z)).RoomNumber).GetFloorHeight();
	auto right = GetPointCollision(Vector3i(item->Pose.Position.x + xf + xr, y, item->Pose.Position.z + zf + zr), GetRoomVector(item->Location, Vector3i(item->Pose.Position.x, y, item->Pose.Position.z)).RoomNumber).GetFloorHeight();

	// If specified, limit vertical search zone only to nearest height
	if (heightLimit && (abs(left - y) > CLICK(0.5f) || abs(right - y) > CLICK(0.5f)))
		return false;

	// Determine allowed slope difference for a given collision radius
	auto slopeDelta = ((float)STEPUP_HEIGHT / (float)BLOCK(1)) * (coll->Setup.Radius * 2);

	// Discard if there is a slope beyond tolerance delta
	if (abs(left - right) >= slopeDelta)
		return false;

	// Discard if ledge is not within distance threshold
	if (abs(coll->NearestLedgeDistance) > OFFSET_RADIUS(coll->Setup.Radius))
		return false;

	// Discard if ledge is not within angle threshold
	if (!TestValidLedgeAngle(item, coll))
		return false;
	
	if (!ignoreHeadroom)
	{
		auto headroom = (coll->Front.Floor + coll->Setup.Height) - coll->Middle.Ceiling;
		if (headroom < CLICK(1))
			return false;
	}
	
	return true;
}

bool TestValidLedgeAngle(ItemInfo* item, CollisionInfo* coll)
{
	return (abs((short)(coll->NearestLedgeAngle - item->Pose.Orientation.y)) <= LARA_GRAB_THRESHOLD);
}

bool TestLaraHang(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	auto angle = lara->Control.MoveAngle;

	// Determine direction of Lara's shimmying (0 if she just hangs still)
	int climbDirection = 0;
	if (lara->Control.MoveAngle == (short)(item->Pose.Orientation.y - ANGLE(90.0f)))
		climbDirection = -1;
	else if (lara->Control.MoveAngle == (short)(item->Pose.Orientation.y + ANGLE(90.0f)))
		climbDirection = 1;

	// Temporarily move item a bit closer to the wall to get more precise coll results
	auto oldPos = item->Pose;
	item->Pose.Position.x += phd_sin(item->Pose.Orientation.y) * coll->Setup.Radius * 0.5f;
	item->Pose.Position.z += phd_cos(item->Pose.Orientation.y) * coll->Setup.Radius * 0.5f;

	// Get height difference with side spaces (left or right, depending on movement direction)
	auto hdif = LaraFloorFront(item, lara->Control.MoveAngle, coll->Setup.Radius * 1.4f);

	// Set stopped flag, if floor height is above footspace which is step size
	auto stopped = hdif < CLICK(0.5f);

	// Set stopped flag, if ceiling height is below headspace which is step size
	if (LaraCeilingFront(item, lara->Control.MoveAngle, coll->Setup.Radius * 1.5f, 0) > -950)
		stopped = true;

	// Restore backup pos after coll tests
	item->Pose = oldPos;

	// Setup coll lara
	lara->Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.ForwardAngle = lara->Control.MoveAngle;
	coll->Setup.ForceSolidStatics = true;

	// When Lara is about to move, use larger embed offset for stabilizing diagonal shimmying)
	int embedOffset = 4;
	if (IsHeld(In::Left) || IsHeld(In::Right))
		embedOffset = 16;

	item->Pose.Position.x += phd_sin(item->Pose.Orientation.y) * embedOffset;
	item->Pose.Position.z += phd_cos(item->Pose.Orientation.y) * embedOffset;

	GetCollisionInfo(coll, item);

	bool result = false;

	if (lara->Control.CanClimbLadder) // Ladder case
	{
		if (IsHeld(In::Action) && item->HitPoints > 0)
		{
			lara->Control.MoveAngle = angle;

			if (!TestLaraHangOnClimbableWall(item, coll))
			{
				if (item->Animation.AnimNumber != LA_LADDER_TO_HANG_RIGHT &&
					item->Animation.AnimNumber != LA_LADDER_TO_HANG_LEFT)
				{
					LaraSnapToEdgeOfBlock(item, coll, GetQuadrant(item->Pose.Orientation.y));
					item->Pose.Position.y = coll->Setup.PrevPosition.y;
					SetAnimation(*item, LA_REACH_TO_HANG, 21);
				}

				result = true;
			}
			else
			{
				if (((item->Animation.AnimNumber == LA_REACH_TO_HANG && item->Animation.FrameNumber == 21) || item->Animation.AnimNumber == LA_HANG_IDLE)  &&
					TestLaraClimbIdle(item, coll))
				{
					item->Animation.TargetState = LS_LADDER_IDLE;
				}
			}
		}
		else // Death or action release
		{
			SetAnimation(*item, LA_FALL_START);
			item->Pose.Position.y += CLICK(1);
			item->Animation.IsAirborne = true;
			item->Animation.Velocity.z = 2;
			item->Animation.Velocity.y = 1;
			lara->Control.HandStatus = HandStatus::Free;
		}
	}
	else // Normal case
	{
		if ((IsHeld(In::Action) && item->HitPoints > 0 && coll->Front.Floor <= 0) ||
			(item->Animation.AnimNumber == LA_LEDGE_JUMP_UP_START || item->Animation.AnimNumber == LA_LEDGE_JUMP_BACK_START)) // TODO: Unhardcode this in a later refactor. @Sezz 2022.10.21)
		{
			if (stopped && hdif > 0 && climbDirection != 0 && (climbDirection > 0 == coll->MiddleLeft.Floor > coll->MiddleRight.Floor))
				stopped = false;

			auto verticalShift = coll->Front.Floor - GameBoundingBox(item).Y1;
			auto x = item->Pose.Position.x;
			auto z = item->Pose.Position.z;

			lara->Control.MoveAngle = angle;

			if (climbDirection != 0)
			{
				auto s = phd_sin(lara->Control.MoveAngle);
				auto c = phd_cos(lara->Control.MoveAngle);
				auto testShift = Vector2(s * coll->Setup.Radius, c * coll->Setup.Radius);

				x += testShift.x;
				z += testShift.y;
			}

			if (TestLaraNearClimbableWall(item, &GetPointCollision(Vector3i(x, item->Pose.Position.y, z), item->RoomNumber).GetBottomSector()))
			{
				if (!TestLaraHangOnClimbableWall(item, coll))
					verticalShift = 0; // Ignore vertical shift if ladder is encountered next block
			}
			else if (!TestValidLedge(item, coll, true))
			{
				if ((climbDirection < 0 && coll->FrontLeft.Floor  != coll->Front.Floor) ||
					(climbDirection > 0 && coll->FrontRight.Floor != coll->Front.Floor))
				{
					stopped = true;
				}
			}

			if (!stopped &&
				coll->Middle.Ceiling < 0 && coll->CollisionType == CollisionType::Front && !coll->HitStatic &&
				abs(verticalShift) < SLOPE_DIFFERENCE && TestValidLedgeAngle(item, coll))
			{
				if (item->Animation.Velocity.z != 0)
					SnapItemToLedge(item, coll);

				item->Pose.Position.y += verticalShift;
			}
			else
			{
				item->Pose.Position = coll->Setup.PrevPosition;

				if (item->Animation.ActiveState == LS_SHIMMY_LEFT ||
					item->Animation.ActiveState == LS_SHIMMY_RIGHT)
				{
					SetAnimation(*item, LA_REACH_TO_HANG, 21);
				}

				result = true;
			}
		}
		else // Death, incorrect ledge or ACTION release
		{
			SetAnimation(*item, LA_JUMP_UP, 9);
			item->Pose.Position.x += coll->Shift.Position.x;
			item->Pose.Position.y += GameBoundingBox(item).Y2 * 1.8f;
			item->Pose.Position.z += coll->Shift.Position.z;
			item->Animation.IsAirborne = true;
			item->Animation.Velocity.z = 2;
			item->Animation.Velocity.y = 1;
			lara->Control.HandStatus = HandStatus::Free;
		}
	}

	return result;
}

bool TestLaraHangJump(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (!IsHeld(In::Action) || lara->Control.HandStatus != HandStatus::Free || coll->HitStatic)
		return false;

	if (CanGrabMonkeySwing(*item, *coll))
	{
		SetAnimation(*item, LA_REACH_TO_MONKEY);
		ResetPlayerFlex(item);
		item->Animation.Velocity.z = 0;
		item->Animation.Velocity.y = 0;
		item->Animation.IsAirborne = false;
		item->Pose.Position.y += coll->Middle.Ceiling + (LARA_HEIGHT_MONKEY - coll->Setup.Height);
		lara->Control.HandStatus = HandStatus::Busy;
		return true;
	}

	if (coll->Middle.Floor < 200 || coll->CollisionType != CollisionType::Front)
		return false;

	int edge;
	auto edgeCatch = TestLaraEdgeCatch(item, coll, &edge);
	if (!edgeCatch)
		return false;

	bool ladder = TestLaraHangOnClimbableWall(item, coll);
	if (!(ladder && edgeCatch) &&
		!(TestValidLedge(item, coll, true, true) && edgeCatch > 0))
	{
		return false;
	}

	if (TestHangSwingIn(item, coll))
	{
		SetAnimation(*item, LA_REACH_TO_HANG_OSCILLATE);
		ResetPlayerFlex(item);
	}
	else
		SetAnimation(*item, LA_REACH_TO_HANG);

	auto bounds = GameBoundingBox(item);
	if (edgeCatch <= 0)
	{
		item->Pose.Position.y = edge - bounds.Y1 - 20;
		item->Pose.Orientation.y = coll->NearestLedgeAngle;
	}
	else
		item->Pose.Position.y += coll->Front.Floor - bounds.Y1 - 20;

	if (ladder)
		SnapItemToGrid(item, coll); // HACK: until fragile ladder code is refactored, we must exactly snap to grid.
	else
		SnapItemToLedge(item, coll, 0.2f);

	ResetPlayerFlex(item);
	item->Animation.IsAirborne = true;
	item->Animation.Velocity.z = 2;
	item->Animation.Velocity.y = 1;
	lara->Control.TurnRate = 0;
	lara->Control.HandStatus = HandStatus::Busy;
	return true;
}

bool TestLaraHangJumpUp(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (!IsHeld(In::Action) || lara->Control.HandStatus != HandStatus::Free || coll->HitStatic)
		return false;

	if (CanGrabMonkeySwing(*item, *coll))
	{
		SetAnimation(*item, LA_JUMP_UP_TO_MONKEY);
		item->Animation.Velocity.z = 0;
		item->Animation.Velocity.y = 0;
		item->Animation.IsAirborne = false;
		item->Pose.Position.y += coll->Middle.Ceiling + (LARA_HEIGHT_MONKEY - coll->Setup.Height);
		lara->Control.HandStatus = HandStatus::Busy;
		return true;
	}

	if (coll->CollisionType != CollisionType::Front)
		return false;

	int edge;
	auto edgeCatch = TestLaraEdgeCatch(item, coll, &edge);
	if (!edgeCatch)
		return false;

	bool ladder = TestLaraHangOnClimbableWall(item, coll);
	if (!(ladder && edgeCatch) &&
		!(TestValidLedge(item, coll, true, true) && edgeCatch > 0))
	{
		return false;
	}

	SetAnimation(*item, LA_REACH_TO_HANG, 12);

	auto bounds = GameBoundingBox(item);
	if (edgeCatch <= 0)
		item->Pose.Position.y = edge - bounds.Y1 + 4;
	else
		item->Pose.Position.y += coll->Front.Floor - bounds.Y1;

	if (ladder)
		SnapItemToGrid(item, coll); // HACK: until fragile ladder code is refactored, we must exactly snap to grid.
	else
		SnapItemToLedge(item, coll);

	ResetPlayerFlex(item);
	item->Animation.Velocity.z = 0;
	item->Animation.Velocity.y = 0;
	item->Animation.IsAirborne = false;
	lara->Control.HandStatus = HandStatus::Busy;
	lara->ExtraTorsoRot = EulerAngles::Identity;
	return true;
}

int TestLaraEdgeCatch(ItemInfo* item, CollisionInfo* coll, int* edge)
{
	auto bounds = GameBoundingBox(item);
	int heightDif = coll->Front.Floor - bounds.Y1;

	if (heightDif < 0 == heightDif + item->Animation.Velocity.y < 0)
	{
		heightDif = item->Pose.Position.y + bounds.Y1;

		if ((heightDif + (int)round(item->Animation.Velocity.y) & 0xFFFFFF00) != (heightDif & 0xFFFFFF00))
		{
			if (item->Animation.Velocity.y > 0)
				*edge = (int)round(heightDif + item->Animation.Velocity.y) & 0xFFFFFF00;
			else
				*edge = heightDif & 0xFFFFFF00;

			return -1;
		}

		return 0;
	}

	if (!TestValidLedge(item, coll, true))
		return 0;

	return 1;
}

bool TestLaraClimbIdle(ItemInfo* item, CollisionInfo* coll)
{
	int shiftRight, shiftLeft;

	if (LaraTestClimbPos(item, coll->Setup.Radius, coll->Setup.Radius + CLICK(0.5f), -700, CLICK(2), &shiftRight) != 1)
		return false;

	if (LaraTestClimbPos(item, coll->Setup.Radius, -(coll->Setup.Radius + CLICK(0.5f)), -700, CLICK(2), &shiftLeft) != 1)
		return false;

	if (shiftRight)
	{
		if (shiftLeft)
		{
			if (shiftRight < 0 != shiftLeft < 0)
				return false;

			if ((shiftRight < 0 && shiftLeft < shiftRight) ||
				(shiftRight > 0 && shiftLeft > shiftRight))
			{
				item->Pose.Position.y += shiftLeft;
				return true;
			}
		}

		item->Pose.Position.y += shiftRight;
	}
	else if (shiftLeft)
		item->Pose.Position.y += shiftLeft;

	return true;
}

bool TestLaraNearClimbableWall(ItemInfo* item, FloorInfo* floor)
{
	if (floor == nullptr)
		floor = &GetPointCollision(*item).GetBottomSector();

	return ((256 << (GetQuadrant(item->Pose.Orientation.y))) & GetClimbFlags(floor));
}

bool TestLaraHangOnClimbableWall(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);
	int shift, result;

	if (!lara->Control.CanClimbLadder)
		return false;

	if (item->Animation.Velocity.y < 0)
		return false;

	// HACK: Climb wall tests are highly fragile and depend on quadrant shifts.
	// Until climb wall tests are fully refactored, we need to recalculate CollisionInfo.

	auto coll2 = *coll;
	coll2.Setup.Mode = CollisionProbeMode::Quadrants;
	GetCollisionInfo(&coll2, item);

	switch (GetQuadrant(item->Pose.Orientation.y))
	{
	case NORTH:
	case SOUTH:
		item->Pose.Position.z += coll2.Shift.Position.z;
		break;

	case EAST:
	case WEST:
		item->Pose.Position.x += coll2.Shift.Position.x;
		break;

	default:
		break;
	}

	auto bounds = GameBoundingBox(item);

	if (lara->Control.MoveAngle != item->Pose.Orientation.y)
	{
		short l = LaraCeilingFront(item, item->Pose.Orientation.y, 0, 0);
		short r = LaraCeilingFront(item, lara->Control.MoveAngle, CLICK(0.5f), 0);

		if (abs(l - r) > SLOPE_DIFFERENCE)
			return false;
	}

	if (LaraTestClimbPos(item, LARA_RADIUS, LARA_RADIUS, bounds.Y1, bounds.GetHeight(), &shift) &&
		LaraTestClimbPos(item, LARA_RADIUS, -LARA_RADIUS, bounds.Y1, bounds.GetHeight(), &shift))
	{
		result = LaraTestClimbPos(item, LARA_RADIUS, 0, bounds.Y1, bounds.GetHeight(), &shift);
		if (result)
		{
			if (result != 1)
				item->Pose.Position.y += shift;

			return true;
		}
	}

	return false;
}

bool TestLaraValidHangPosition(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	// Get incoming ledge height and own Lara's upper bound.
	// First one will be negative while first one is positive.
	// Difference between two indicates difference in height between ledges.
	auto frontFloor = GetPointCollision(*item, lara->Control.MoveAngle, coll->Setup.Radius + CLICK(0.5f), -LARA_HEIGHT).GetFloorHeight();
	auto laraUpperBound = item->Pose.Position.y - coll->Setup.Height;

	// If difference is above 1/2 click, return false (ledge is out of reach).
	if (abs(frontFloor - laraUpperBound) > CLICK(0.5f))
		return false;

	// Embed Lara into wall to make collision test succeed
	item->Pose.Position.x += phd_sin(item->Pose.Orientation.y) * 8;
	item->Pose.Position.z += phd_cos(item->Pose.Orientation.y) * 8;

	// Setup new GCI call
	lara->Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -CLICK(2);
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.Mode = CollisionProbeMode::FreeFlat;
	coll->Setup.ForwardAngle = lara->Control.MoveAngle;

	GetCollisionInfo(coll, item);

	// Filter out narrow ceiling spaces, no collision cases and statics in front.
	if (coll->Middle.Ceiling >= 0 || coll->CollisionType != CollisionType::Front || coll->HitStatic)
		return false;

	// Finally, do ordinary ledge checks (slope difference etc.)
	return TestValidLedge(item, coll);
}

CornerType TestLaraHangCorner(ItemInfo* item, CollisionInfo* coll, float testAngle)
{
	auto* lara = GetLaraInfo(item);

	// Lara isn't in stop state yet, bypass test
	if (item->Animation.AnimNumber != LA_REACH_TO_HANG && item->Animation.AnimNumber != LA_HANG_IDLE)
		return CornerType::None;

	// Static is in the way, bypass test
	if (coll->HitStatic)
		return CornerType::None;

	// INNER CORNER TESTS

	// Backup old Lara position and frontal collision
	auto oldPos = item->Pose;
	auto oldMoveAngle = lara->Control.MoveAngle;

	auto cornerResult = TestItemAtNextCornerPosition(item, coll, testAngle, false);

	// Do further testing only if test angle is equal to resulting edge angle
	if (cornerResult.Success)
	{
		// Get bounding box height for further ledge height calculations
		auto bounds = GameBoundingBox(item);

		// Store next position
		item->Pose = cornerResult.RealPositionResult;
		lara->Context.NextCornerPos.Position = Vector3i(
			item->Pose.Position.x,
			GetPointCollision(*item, item->Pose.Orientation.y, coll->Setup.Radius + 16, -(coll->Setup.Height + CLICK(0.5f))).GetFloorHeight() + abs(bounds.Y1),
			item->Pose.Position.z
		);
		lara->Context.NextCornerPos.Orientation.y = item->Pose.Orientation.y;
		lara->Control.MoveAngle = item->Pose.Orientation.y;

		item->Pose = cornerResult.ProbeResult;
		auto result = TestLaraValidHangPosition(item, coll);

		// Restore original item positions
		item->Pose = oldPos;
		lara->Control.MoveAngle = oldMoveAngle;

		if (result)
			return CornerType::Inner;

		if (lara->Control.CanClimbLadder)
		{
			auto& angleSet = testAngle > 0 ? LeftExtRightIntTab : LeftIntRightExtTab;
			if (GetClimbFlags(lara->Context.NextCornerPos.Position.x, item->Pose.Position.y, lara->Context.NextCornerPos.Position.z, item->RoomNumber) & (short)angleSet[GetQuadrant(item->Pose.Orientation.y)])
			{
				lara->Context.NextCornerPos.Position.y = item->Pose.Position.y; // Restore original Y pos for ladder tests because we don't snap to ledge height in such case.
				return CornerType::Inner;
			}
		}
	}

	// Restore original item positions
	item->Pose = oldPos;
	lara->Control.MoveAngle = oldMoveAngle;

	// OUTER CORNER TESTS

	// Test if there's a material obstacles blocking outer corner pathway
	if ((LaraFloorFront(item, item->Pose.Orientation.y + ANGLE(testAngle), coll->Setup.Radius + CLICK(1)) < 0) ||
		(LaraCeilingFront(item, item->Pose.Orientation.y + ANGLE(testAngle), coll->Setup.Radius + CLICK(1), coll->Setup.Height) > 0))
		return CornerType::None;

	// Last chance for possible diagonal vs. non-diagonal cases: ray test
	if (!LaraPositionOnLOS(item, item->Pose.Orientation.y + ANGLE(testAngle), coll->Setup.Radius + CLICK(1)))
		return CornerType::None;

	cornerResult = TestItemAtNextCornerPosition(item, coll, testAngle, true);

	// Additional test if there's a material obstacles blocking outer corner pathway
	if ((LaraFloorFront(item, item->Pose.Orientation.y, 0) < 0) ||
		(LaraCeilingFront(item, item->Pose.Orientation.y, 0, coll->Setup.Height) > 0))
		cornerResult.Success = false;

	// Do further testing only if test angle is equal to resulting edge angle
	if (cornerResult.Success)
	{
		// Get bounding box height for further ledge height calculations
		auto bounds = GameBoundingBox(item);

		// Store next position
		item->Pose = cornerResult.RealPositionResult;
		lara->Context.NextCornerPos.Position.x = item->Pose.Position.x;
		lara->Context.NextCornerPos.Position.y = GetPointCollision(*item, item->Pose.Orientation.y, coll->Setup.Radius * 1.25f, -(abs(bounds.Y1) + LARA_HEADROOM)).GetFloorHeight() + abs(bounds.Y1);
		lara->Context.NextCornerPos.Position.z = item->Pose.Position.z;
		lara->Context.NextCornerPos.Orientation.y = item->Pose.Orientation.y;
		lara->Control.MoveAngle = item->Pose.Orientation.y;

		item->Pose = cornerResult.ProbeResult;
		auto result = TestLaraValidHangPosition(item, coll);

		// Restore original item positions
		item->Pose = oldPos;
		lara->Control.MoveAngle = oldMoveAngle;

		if (result)
			return CornerType::Outer;

		if (lara->Control.CanClimbLadder)
		{
			auto& angleSet = testAngle > 0 ? LeftIntRightExtTab : LeftExtRightIntTab;
			if (GetClimbFlags(lara->Context.NextCornerPos.Position.x, item->Pose.Position.y, lara->Context.NextCornerPos.Position.z, item->RoomNumber) & (short)angleSet[GetQuadrant(item->Pose.Orientation.y)])
			{
				lara->Context.NextCornerPos.Position.y = item->Pose.Position.y; // Restore original Y pos for ladder tests because we don't snap to ledge height in such case.
				return CornerType::Outer;
			}
		}
	}

	// Restore original item positions
	item->Pose = oldPos;
	lara->Control.MoveAngle = oldMoveAngle;

	return CornerType::None;
}

CornerTestResult TestItemAtNextCornerPosition(ItemInfo* item, CollisionInfo* coll, float angle, bool outer)
{
	auto* lara = GetLaraInfo(item);

	auto result = CornerTestResult();

	// Determine real turning angle
	auto turnAngle = outer ? angle : -angle;

	// Backup previous position into array
	Pose pos[3] = { item->Pose, item->Pose, item->Pose };

	// Do a two-step rotation check. First step is real resulting position, and second step is probing
	// position. We need this because checking at exact ending position does not always return
	// correct results with nearest ledge angle.

	for (int i = 0; i < 2; i++)
	{
		// Determine collision box anchor point and rotate collision box around this anchor point.
		// Then determine new test position from centerpoint of new collision box position.

		// Push back item a bit to compensate for possible edge ledge cases
		pos[i].Position.x -= round((coll->Setup.Radius * (outer ? -0.2f : 0.2f)) * phd_sin(pos[i].Orientation.y));
		pos[i].Position.z -= round((coll->Setup.Radius * (outer ? -0.2f : 0.2f)) * phd_cos(pos[i].Orientation.y));

		// Move item at the distance of full collision diameter plus half-radius margin to movement direction 
		pos[i].Position.x += round((coll->Setup.Radius * (i == 0 ? 2.0f : 2.5f)) * phd_sin(lara->Control.MoveAngle));
		pos[i].Position.z += round((coll->Setup.Radius * (i == 0 ? 2.0f : 2.5f)) * phd_cos(lara->Control.MoveAngle));

		// Determine anchor point
		auto cX = pos[i].Position.x + round(coll->Setup.Radius * phd_sin(pos[i].Orientation.y));
		auto cZ = pos[i].Position.z + round(coll->Setup.Radius * phd_cos(pos[i].Orientation.y));
		cX += (coll->Setup.Radius * phd_sin(pos[i].Orientation.y + ANGLE(90.0f * -std::copysign(1.0f, angle))));
		cZ += (coll->Setup.Radius * phd_cos(pos[i].Orientation.y + ANGLE(90.0f * -std::copysign(1.0f, angle))));

		// Determine distance from anchor point to new item position
		auto dist = Vector2(pos[i].Position.x, pos[i].Position.z) - Vector2(cX, cZ);
		auto s = phd_sin(ANGLE(turnAngle));
		auto c = phd_cos(ANGLE(turnAngle));

		// Shift item to a new anchor point
		pos[i].Position.x = dist.x * c - dist.y * s + cX;
		pos[i].Position.z = dist.x * s + dist.y * c + cZ;

		// Virtually rotate item to new angle
		short newAngle = pos[i].Orientation.y - ANGLE(turnAngle);
		pos[i].Orientation.y = newAngle;

		// Snap to nearest ledge, if any.
		item->Pose = pos[i];
		SnapItemToLedge(item, coll, item->Pose.Orientation.y);

		// Copy resulting position to an array and restore original item position.
		pos[i] = item->Pose;
		item->Pose = pos[2];

		if (i == 1) // Both passes finished, construct the result.
		{
			result.RealPositionResult = pos[0];
			result.ProbeResult = pos[1];
			result.Success = newAngle == pos[i].Orientation.y;
		}
	}

	return result;
}

bool TestHangSwingIn(ItemInfo* item, CollisionInfo* coll)
{
	int vPos = item->Pose.Position.y;
	auto pointColl = GetPointCollision(*item, item->Pose.Orientation.y, OFFSET_RADIUS(coll->Setup.Radius) + item->Animation.Velocity.z);

	// 1) Test for wall.
	if (pointColl.GetFloorHeight() == NO_HEIGHT)
		return false;

	// 2) Test leg space.
	if ((pointColl.GetFloorHeight() - vPos) > 0 &&
		(pointColl.GetCeilingHeight() - vPos) < -CLICK(1.6f))
	{
		return true;
	}

	return false;
}

bool TestLaraHangSideways(ItemInfo* item, CollisionInfo* coll, short angle)
{
	auto* lara = GetLaraInfo(item);

	auto oldPos = item->Pose;

	lara->Control.MoveAngle = item->Pose.Orientation.y + angle;

	static constexpr auto sidewayTestDistance = 16;
	item->Pose.Position.x += phd_sin(lara->Control.MoveAngle) * sidewayTestDistance;
	item->Pose.Position.z += phd_cos(lara->Control.MoveAngle) * sidewayTestDistance;

	coll->Setup.PrevPosition.y = item->Pose.Position.y;

	bool res = TestLaraHang(item, coll);

	item->Pose = oldPos;

	return !res;
}

bool TestLaraWall(const ItemInfo* item, float dist, float height)
{
	auto origin = GameVector(
		Geometry::TranslatePoint(item->Pose.Position, item->Pose.Orientation.y, 0.0f, height),
		item->RoomNumber);
	auto target = GameVector(
		Geometry::TranslatePoint(item->Pose.Position, item->Pose.Orientation.y, dist, height),
		item->RoomNumber);

	return !LOS(&origin, &target);
}

bool TestLaraFacingCorner(const ItemInfo* item, short headingAngle, float dist)
{
	short angleLeft = headingAngle - ANGLE(15.0f);
	short angleRight = headingAngle + ANGLE(15.0f);

	auto start = GameVector(
		item->Pose.Position.x,
		item->Pose.Position.y - STEPUP_HEIGHT,
		item->Pose.Position.z,
		item->RoomNumber);

	auto end1 = GameVector(
		item->Pose.Position.x + dist * phd_sin(angleLeft),
		item->Pose.Position.y - STEPUP_HEIGHT,
		item->Pose.Position.z + dist * phd_cos(angleLeft),
		item->RoomNumber);

	auto end2 = GameVector(
		item->Pose.Position.x + dist * phd_sin(angleRight),
		item->Pose.Position.y - STEPUP_HEIGHT,
		item->Pose.Position.z + dist * phd_cos(angleRight),
		item->RoomNumber);

	bool result1 = LOS(&start, &end1);
	bool result2 = LOS(&start, &end2);
	return (!result1 && !result2);
}

bool LaraPositionOnLOS(ItemInfo* item, short angle, int distance)
{
	auto start1 = GameVector(
		item->Pose.Position.x,
		item->Pose.Position.y - LARA_HEADROOM,
		item->Pose.Position.z,
		item->RoomNumber);

	auto start2 = GameVector(
		item->Pose.Position.x,
		item->Pose.Position.y - LARA_HEIGHT + LARA_HEADROOM,
		item->Pose.Position.z,
		item->RoomNumber);
	
	auto end1 = GameVector(
		item->Pose.Position.x + distance * phd_sin(angle),
		item->Pose.Position.y - LARA_HEADROOM,
		item->Pose.Position.z + distance * phd_cos(angle),
		item->RoomNumber);

	auto end2 = GameVector(
		item->Pose.Position.x + distance * phd_sin(angle),
		item->Pose.Position.y - LARA_HEIGHT + LARA_HEADROOM,
		item->Pose.Position.z + distance * phd_cos(angle),
		item->RoomNumber);

	auto result1 = LOS(&start1, &end1);
	auto result2 = LOS(&start2, &end2);

	return (result1 && result2);
}

int LaraFloorFront(ItemInfo* item, short angle, int distance)
{
	auto pointColl = GetPointCollision(*item, angle, distance, -LARA_HEIGHT);

	if (pointColl.GetFloorHeight() == NO_HEIGHT)
		return pointColl.GetFloorHeight();

	return (pointColl.GetFloorHeight() - item->Pose.Position.y);
}

int LaraCeilingFront(ItemInfo* item, short angle, int distance, int height)
{
	auto pointColl = GetPointCollision(*item, angle, distance, -height);

	if (pointColl.GetCeilingHeight() == NO_HEIGHT)
		return pointColl.GetCeilingHeight();

	return ((pointColl.GetCeilingHeight() + height) - item->Pose.Position.y);
}

bool TestPlayerWaterStepOut(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Get point collision.
	auto pointColl = GetPointCollision(*item);
	int vPos = item->Pose.Position.y;

	if (coll->CollisionType == CollisionType::Front ||
		pointColl.IsSteepFloor() ||
		(pointColl.GetFloorHeight() - vPos) <= 0)
	{
		return false;
	}

	if ((pointColl.GetFloorHeight() - vPos) >= -CLICK(0.5f))
	{
		SetAnimation(*item, LA_STAND_IDLE);
	}
	else
	{
		SetAnimation(*item, LA_ONWATER_TO_WADE_1_STEP);
		item->Animation.TargetState = LS_IDLE;
	}

	item->Pose.Position.y = pointColl.GetFloorHeight();
	UpdateLaraRoom(item, -(STEPUP_HEIGHT - 3));

	ResetPlayerLean(item);
	item->Animation.Velocity.y = 0.0f;
	item->Animation.Velocity.z = 0.0f;
	item->Animation.IsAirborne = false;
	player.Control.WaterStatus = WaterStatus::Wade;

	return true;
}

bool TestLaraWaterClimbOut(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);
	const auto& settings = g_GameFlow->GetSettings()->Animations;

	if (coll->CollisionType != CollisionType::Front || !IsHeld(In::Action))
		return false;

	if (lara->Control.HandStatus != HandStatus::Free &&
		(lara->Control.HandStatus != HandStatus::WeaponReady || lara->Control.Weapon.GunType != LaraWeaponType::Flare))
	{
		return false;
	}

	if (coll->Middle.Ceiling > -STEPUP_HEIGHT)
		return false;

	// HACK: Probe at incremetal height steps to account for room stacks. -- Sezz 2024.10.28
	int frontFloor = NO_HEIGHT;
	
	bool hasLedge = false;
	int yOffset = CLICK(1.25f);
	while (yOffset > -CLICK(2))
	{
		auto pointColl = GetPointCollision(*item, item->Pose.Orientation.y, BLOCK(0.2f), yOffset);

		frontFloor = pointColl.GetFloorHeight() - item->Pose.Position.y;
		if (frontFloor > -CLICK(2) &&
			frontFloor <= (CLICK(1.25f) - 4))
		{
			hasLedge = true;
			break;
		}

		yOffset -= CLICK(0.5f);
	}

	if (!hasLedge)
		return false;

	if (!TestValidLedge(item, coll))
		return false;

	TestForObjectOnLedge(item, coll);
	if (coll->HitStatic)
		return false;

	auto probe = GetPointCollision(*item, coll->Setup.ForwardAngle, CLICK(2), -CLICK(1));
	int headroom = probe.GetFloorHeight() - probe.GetCeilingHeight();

	if (frontFloor <= -CLICK(1))
	{
		if (headroom < LARA_HEIGHT)
		{
			if (settings.CrawlExtended)
				SetAnimation(*item, LA_ONWATER_TO_CROUCH_1_STEP);
			else
				return false;
		}
		else
			SetAnimation(*item, LA_ONWATER_TO_STAND_1_STEP);
	}
	else if (frontFloor > CLICK(0.5f))
	{
		if (headroom < LARA_HEIGHT)
		{
			if (settings.CrawlExtended)
				SetAnimation(*item, LA_ONWATER_TO_CROUCH_M1_STEP);
			else
				return false;
		}
		else
			SetAnimation(*item, LA_ONWATER_TO_STAND_M1_STEP);
	}

	else
	{
		if (headroom < LARA_HEIGHT)
		{
			if (settings.CrawlExtended)
				SetAnimation(*item, LA_ONWATER_TO_CROUCH_0_STEP);
			else
				return false;
		}
		else
			SetAnimation(*item, LA_ONWATER_TO_STAND_0_STEP);
	}

	if (coll->Front.Bridge == NO_VALUE)
		UpdateLaraRoom(item, -LARA_HEIGHT / 2);
	else
		UpdateLaraRoom(item, -LARA_HEIGHT);

	SnapItemToLedge(item, coll, 1.7f);

	item->Pose.Position.y += frontFloor - 5;
	item->Animation.ActiveState = LS_ONWATER_EXIT;
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.z = 0;
	item->Animation.Velocity.y = 0;
	lara->Control.TurnRate = 0;
	lara->Control.HandStatus = HandStatus::Busy;
	lara->Control.WaterStatus = WaterStatus::Dry;
	return true;
}

bool TestLaraLadderClimbOut(ItemInfo* item, CollisionInfo* coll) // NEW function for water to ladder move
{
	auto* lara = GetLaraInfo(item);

	if (!IsHeld(In::Action) || !lara->Control.CanClimbLadder || coll->CollisionType != CollisionType::Front)
	{
		return false;
	}

	if (lara->Control.HandStatus != HandStatus::Free &&
		(lara->Control.HandStatus != HandStatus::WeaponReady || lara->Control.Weapon.GunType != LaraWeaponType::Flare))
	{
		return false;
	}

	// HACK: Reduce probe radius, because free forward probe mode makes ladder tests to fail in some cases.
	coll->Setup.Radius *= 0.8f; 

	if (!TestLaraClimbIdle(item, coll))
		return false;

	short facing = item->Pose.Orientation.y;

	if (facing >= -ANGLE(35.0f) && facing <= ANGLE(35.0f))
		facing = 0;
	else if (facing >= ANGLE(55.0f) && facing <= ANGLE(125.0f))
		facing = ANGLE(90.0f);
	else if (facing >= ANGLE(145.0f) || facing <= -ANGLE(145.0f))
		facing = ANGLE(180.0f);
	else if (facing >= -ANGLE(125.0f) && facing <= -ANGLE(55.0f))
		facing = -ANGLE(90.0f);

	if (facing & 0x3FFF)
		return false;

	switch ((unsigned short)facing / ANGLE(90.0f))
	{
	case NORTH:
		item->Pose.Position.z = (item->Pose.Position.z | WALL_MASK) - LARA_RADIUS - 1;
		break;

	case EAST:
		item->Pose.Position.x = (item->Pose.Position.x | WALL_MASK) - LARA_RADIUS - 1;
		break;

	case SOUTH:
		item->Pose.Position.z = (item->Pose.Position.z & -BLOCK(1)) + LARA_RADIUS + 1;
		break;

	case WEST:
		item->Pose.Position.x = (item->Pose.Position.x & -BLOCK(1)) + LARA_RADIUS + 1;
		break;
	}

	SetAnimation(*item, LA_ONWATER_IDLE);
	item->Animation.TargetState = LS_LADDER_IDLE;
	AnimateItem(*item);

	item->Pose.Position.y -= 10; // Otherwise she falls back into the water.
	item->Pose.Orientation.x = 0;
	item->Pose.Orientation.y = facing;
	item->Pose.Orientation.z = 0;
	item->Animation.Velocity.z = 0;
	item->Animation.Velocity.y = 0;
	item->Animation.IsAirborne = false;
	lara->Control.TurnRate = 0;
	lara->Control.HandStatus = HandStatus::Busy;
	lara->Control.WaterStatus = WaterStatus::Dry;
	return true;
}

void TestLaraWaterDepth(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	auto pointColl = GetPointCollision(*item);

	if (pointColl.GetWaterBottomHeight() == NO_HEIGHT)
	{
		item->Animation.Velocity.y = 0.0f;
		item->Pose.Position = coll->Setup.PrevPosition;
	}
	else if (pointColl.GetWaterBottomHeight() <= (LARA_HEIGHT - (LARA_HEADROOM / 2)))
	{
		SetAnimation(*item, LA_UNDERWATER_TO_STAND);
		ResetPlayerLean(item);
		item->Animation.TargetState = LS_IDLE;
		item->Pose.Position.y = pointColl.GetFloorHeight();
		item->Animation.IsAirborne = false;
		item->Animation.Velocity.y = 0.0f;
		item->Animation.Velocity.z = 0.0f;
		player.Control.WaterStatus = WaterStatus::Wade;
	}
}

bool TestLaraWeaponType(LaraWeaponType refWeaponType, const std::vector<LaraWeaponType>& weaponTypeList)
{
	return Contains(weaponTypeList, refWeaponType);
}

static std::vector<LaraWeaponType> StandingWeaponTypes
{
	LaraWeaponType::Shotgun,
	LaraWeaponType::HK,
	LaraWeaponType::Crossbow,
	LaraWeaponType::GrenadeLauncher,
	LaraWeaponType::HarpoonGun,
	LaraWeaponType::RocketLauncher,
	LaraWeaponType::Snowmobile
};

bool IsStandingWeapon(const ItemInfo* item, LaraWeaponType weaponType)
{
	return (TestLaraWeaponType(weaponType, StandingWeaponTypes) || GetLaraInfo(*item).Weapons[(int)weaponType].HasLasersight);
}

bool IsVaultState(int state)
{
	static const std::vector<int> vaultStates
	{
		LS_VAULT,
		LS_VAULT_2_STEPS,
		LS_VAULT_3_STEPS,
		LS_VAULT_1_STEP_CROUCH,
		LS_VAULT_2_STEPS_CROUCH,
		LS_VAULT_3_STEPS_CROUCH,
		LS_AUTO_JUMP
	};
	return TestState(state, vaultStates);
}

bool IsJumpState(int state)
{
	static const std::vector<int> jumpStates
	{
		LS_JUMP_FORWARD,
		LS_JUMP_BACK,
		LS_JUMP_LEFT,
		LS_JUMP_RIGHT,
		LS_JUMP_UP,
		LS_FALL_BACK,
		LS_REACH,
		LS_SWAN_DIVE,
		LS_FREEFALL_DIVE,
		LS_FREEFALL
	};
	return TestState(state, jumpStates);
}

bool IsRunJumpQueueableState(int state)
{
	static const auto RUN_JUMP_QUEUABLE_STATES = std::vector<int>
	{
		LS_RUN_FORWARD,
		LS_SPRINT,
		LS_STEP_UP,
		LS_STEP_DOWN
	};

	return TestState(state, RUN_JUMP_QUEUABLE_STATES);
}

bool IsRunJumpCountableState(int state)
{
	static const std::vector<int> runningJumpTimerStates
	{
		LS_WALK_FORWARD,
		LS_RUN_FORWARD,
		LS_SPRINT,
		LS_SPRINT_DIVE,
		LS_JUMP_FORWARD
	};
	return TestState(state, runningJumpTimerStates);
}

std::optional<VaultTestResult> TestLaraVaultTolerance(ItemInfo* item, CollisionInfo* coll, VaultTestSetup testSetup)
{
	auto* lara = GetLaraInfo(item);

	int distance = OFFSET_RADIUS(coll->Setup.Radius);
	auto probeFront = GetPointCollision(*item, coll->NearestLedgeAngle, distance, -coll->Setup.Height);
	auto probeMiddle = GetPointCollision(*item);

	bool isSwamp = TestEnvironment(ENV_FLAG_SWAMP, item);
	bool swampTooDeep = testSetup.CheckSwampDepth ? (isSwamp && lara->Context.WaterSurfaceDist < -CLICK(3)) : isSwamp;
	int y = isSwamp ? item->Pose.Position.y : probeMiddle.GetFloorHeight(); // HACK: Avoid cheese when in the midst of performing a step. Can be done better. @Sezz 2022.04.08	

	// Check swamp depth (if applicable).
	if (swampTooDeep)
		return std::nullopt;

	// NOTE: Where the point/room probe finds that
	// a) the "wall" in front is formed by a ceiling, or
	// b) the space between the floor and ceiling is a clamp (i.e. it is too narrow),
	// any potentially climbable floor in a room above will be missed. The following block considers this.
	
	// Raise y position of point/room probe by increments of CLICK(0.5f) to find potential vault ledge.
	int yOffset = testSetup.LowerFloorBound;
	while (((probeFront.GetCeilingHeight() - y) > -coll->Setup.Height ||								// Ceiling is below Lara's height...
			abs(probeFront.GetCeilingHeight() - probeFront.GetFloorHeight()) <= testSetup.ClampMin ||		// OR clamp is too small
			abs(probeFront.GetCeilingHeight() - probeFront.GetFloorHeight()) > testSetup.ClampMax) &&		// OR clamp is too large (future-proofing; not possible right now).
		yOffset > (testSetup.UpperFloorBound - coll->Setup.Height))									// Offset is not too high.
	{
		probeFront = GetPointCollision(*item, coll->NearestLedgeAngle, distance, yOffset);
		yOffset -= std::max<int>(CLICK(0.5f), testSetup.ClampMin);
	}

	// Discard walls.
	if (probeFront.GetFloorHeight() == NO_HEIGHT)
		return std::nullopt;

	// Assess point/room collision.
	if ((probeFront.GetFloorHeight() - y) < testSetup.LowerFloorBound &&							// Within lower floor bound.
		(probeFront.GetFloorHeight() - y) >= testSetup.UpperFloorBound &&							// Within upper floor bound.
		abs(probeFront.GetCeilingHeight() - probeFront.GetFloorHeight()) > testSetup.ClampMin &&	// Within clamp min.
		abs(probeFront.GetCeilingHeight() - probeFront.GetFloorHeight()) <= testSetup.ClampMax &&	// Within clamp max.
		abs(probeMiddle.GetCeilingHeight() - probeFront.GetFloorHeight()) >= testSetup.GapMin)		// Gap is optically permissive.
	{
		return VaultTestResult{ probeFront.GetFloorHeight() };
	}

	return std::nullopt;
}

std::optional<VaultTestResult> TestLaraVault2Steps(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: (-STEPUP_HEIGHT, -CLICK(2.5f)]
	// Clamp range: (-LARA_HEIGHT, -MAX_HEIGHT]

	VaultTestSetup testSetup
	{
		-STEPUP_HEIGHT, int(-CLICK(2.5f)),
		LARA_HEIGHT, -MAX_HEIGHT,
		CLICK(1)
	};

	auto testResult = TestLaraVaultTolerance(item, coll, testSetup);
	if (!testResult.has_value())
		return std::nullopt;

	testResult->Height += CLICK(2);
	testResult->SetBusyHands = true;
	testResult->SnapToLedge = true;
	testResult->SetJumpVelocity = false;
	return testResult;
}

std::optional<VaultTestResult> TestLaraVault3Steps(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: (-CLICK(2.5f), -CLICK(3.5f)]
	// Clamp range: (-LARA_HEIGHT, -MAX_HEIGHT]

	VaultTestSetup testSetup
	{
		int(-CLICK(2.5f)), int(-CLICK(3.5f)),
		LARA_HEIGHT, -MAX_HEIGHT,
		int(CLICK(1)),
	};

	auto testResult = TestLaraVaultTolerance(item, coll, testSetup);
	if (!testResult.has_value())
		return std::nullopt;

	testResult->Height += CLICK(3);
	testResult->SetBusyHands = true;
	testResult->SnapToLedge = true;
	testResult->SetJumpVelocity = false;
	return testResult;
}

std::optional<VaultTestResult> TestLaraVault1StepToCrouch(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: (0, -STEPUP_HEIGHT]
	// Clamp range: (-LARA_HEIGHT_CRAWL, -LARA_HEIGHT]

	VaultTestSetup testSetup
	{
		0, -STEPUP_HEIGHT,
		LARA_HEIGHT_CRAWL, LARA_HEIGHT,
		CLICK(1),
	};

	auto testResult = TestLaraVaultTolerance(item, coll, testSetup);
	if (!testResult.has_value())
		return std::nullopt;

	testResult->Height += CLICK(1);
	testResult->SetBusyHands = true;
	testResult->SnapToLedge = true;
	testResult->SetJumpVelocity = false;
	return testResult;
}

std::optional<VaultTestResult> TestLaraVault2StepsToCrouch(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: (-STEPUP_HEIGHT, -CLICK(2.5f)]
	// Clamp range: (-LARA_HEIGHT_CRAWL, -LARA_HEIGHT]

	VaultTestSetup testSetup
	{
		-STEPUP_HEIGHT, int(-CLICK(2.5f)),
		LARA_HEIGHT_CRAWL, LARA_HEIGHT,
		int(CLICK(1))
	};

	auto testResult = TestLaraVaultTolerance(item, coll, testSetup);
	if (!testResult.has_value())
		return std::nullopt;

	testResult->Height += CLICK(2);
	testResult->SetBusyHands = true;
	testResult->SnapToLedge = true;
	testResult->SetJumpVelocity = false;
	return testResult;
}

std::optional<VaultTestResult> TestLaraVault3StepsToCrouch(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: (-CLICK(2.5f), -CLICK(3.5f)]
	// Clamp range: (-LARA_HEIGHT_CRAWL, -LARA_HEIGHT]

	VaultTestSetup testSetup
	{
		int(-CLICK(2.5f)), int(-CLICK(3.5f)),
		LARA_HEIGHT_CRAWL, LARA_HEIGHT,
		int(CLICK(1)),
	};

	auto testResult = TestLaraVaultTolerance(item, coll, testSetup);
	if (!testResult.has_value())
		return std::nullopt;

	testResult->Height += CLICK(3);
	testResult->SetBusyHands = true;
	testResult->SnapToLedge = true;
	testResult->SetJumpVelocity = false;
	return testResult;
}

std::optional<VaultTestResult> TestLaraLedgeAutoJump(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: (-CLICK(3.5f), -CLICK(7.5f)]
	// Clamp range: (-CLICK(0.1f), -MAX_HEIGHT]

	VaultTestSetup testSetup
	{
		int(-CLICK(3.5f)), int(-CLICK(7.5f)),
		int(CLICK(0.1f)) /* TODO: Is this enough hand room?*/, -MAX_HEIGHT,
		int(CLICK(0.1f)),
		false
	};

	auto testResult = TestLaraVaultTolerance(item, coll, testSetup);
	if (!testResult.has_value())
		return std::nullopt;

	testResult->SetBusyHands = false;
	testResult->SnapToLedge = true;
	testResult->SetJumpVelocity = true;
	return testResult;
}

std::optional<VaultTestResult> TestLaraLadderAutoJump(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	int y = item->Pose.Position.y;
	int distance = OFFSET_RADIUS(coll->Setup.Radius);
	auto probeFront = GetPointCollision(*item, coll->NearestLedgeAngle, distance, -coll->Setup.Height);
	auto probeMiddle = GetPointCollision(*item);

	// Check ledge angle.
	if (!TestValidLedgeAngle(item, coll))
		return std::nullopt;

	if (lara->Control.CanClimbLadder &&								// Ladder sector flag set.
		(probeMiddle.GetCeilingHeight() - y) <= -CLICK(6.5f) &&		// Within lowest middle ceiling bound. (Synced with TestLaraLadderMount())
		((probeFront.GetFloorHeight() - y) <= -CLICK(6.5f) ||			// Floor height is appropriate, OR
			(probeFront.GetCeilingHeight() - y) > -CLICK(6.5f)) &&		// Ceiling height is appropriate. (Synced with TestLaraLadderMount())
		coll->NearestLedgeDistance <= coll->Setup.Radius)			// Appropriate distance from wall.
	{
		return VaultTestResult{ probeMiddle.GetCeilingHeight(), false, true, true };
	}

	return std::nullopt;
}

std::optional<VaultTestResult> TestLaraLadderMount(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	int y = item->Pose.Position.y;
	int distance = OFFSET_RADIUS(coll->Setup.Radius);
	auto probeFront = GetPointCollision(*item, coll->NearestLedgeAngle, distance, -coll->Setup.Height);
	auto probeMiddle = GetPointCollision(*item);

	// Check ledge angle.
	if (!TestValidLedgeAngle(item, coll))
		return std::nullopt;

	if (lara->Control.CanClimbLadder &&							// Ladder sector flag set.
		(probeMiddle.GetCeilingHeight() - y) <= -CLICK(4.5f) &&	// Within lower middle ceiling bound.
		(probeMiddle.GetCeilingHeight() - y) > -CLICK(6.5f) &&	// Within upper middle ceiling bound.
		(probeMiddle.GetFloorHeight() - y) > -CLICK(6.5f) &&		// Within upper middle floor bound. (Synced with TestLaraAutoJump())
		(probeFront.GetCeilingHeight() - y) <= -CLICK(4.5f) &&	// Within lowest front ceiling bound.
		coll->NearestLedgeDistance <= coll->Setup.Radius)		// Appropriate distance from wall.
	{
		return VaultTestResult{ NO_HEIGHT, true, true, false };
	}

	return std::nullopt;
}

std::optional<VaultTestResult> TestLaraAutoMonkeySwingJump(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	int y = item->Pose.Position.y;
	auto probe = GetPointCollision(*item);

	if (lara->Control.CanMonkeySwing &&							// Monkey swing sector flag set.
		(probe.GetCeilingHeight() - y) < -LARA_HEIGHT_MONKEY &&	// Within lower ceiling bound.
		(probe.GetCeilingHeight() - y) >= -CLICK(7))				// Within upper ceiling bound.
	{
		return VaultTestResult{ probe.GetCeilingHeight(), false, false, true };
	}

	return std::nullopt;
}

std::optional<VaultTestResult> TestLaraVault(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);
	auto& settings = g_GameFlow->GetSettings()->Animations;

	if (lara->Control.HandStatus != HandStatus::Free)
		return std::nullopt;

	if (TestEnvironment(ENV_FLAG_SWAMP, item) && lara->Context.WaterSurfaceDist < -CLICK(3))
		return std::nullopt;

	std::optional<VaultTestResult> vaultResult = {};

	// Attempt ledge vault.
	if (TestValidLedge(item, coll))
	{
		// Vault to crouch up one step.
		vaultResult = TestLaraVault1StepToCrouch(item, coll);
		if (vaultResult.has_value())
		{
			vaultResult->TargetState = LS_VAULT_1_STEP_CROUCH;
			if (!TestStateDispatch(*item, vaultResult->TargetState))
				return std::nullopt;

			return vaultResult;
		}

		// Vault to stand up two steps.
		vaultResult = TestLaraVault2Steps(item, coll);
		if (vaultResult.has_value())
		{
			vaultResult->TargetState = LS_VAULT_2_STEPS;
			if (!TestStateDispatch(*item, vaultResult->TargetState))
				return std::nullopt;

			return vaultResult;
		}

		// Vault to crouch up two steps.
		vaultResult = TestLaraVault2StepsToCrouch(item, coll);
		if (vaultResult.has_value() && settings.CrawlExtended)
		{
			vaultResult->TargetState = LS_VAULT_2_STEPS_CROUCH;
			if (!TestStateDispatch(*item, vaultResult->TargetState))
				return std::nullopt;

			return vaultResult;
		}

		// Vault to stand up three steps.
		vaultResult = TestLaraVault3Steps(item, coll);
		if (vaultResult.has_value())
		{
			vaultResult->TargetState = LS_VAULT_3_STEPS;
			if (!TestStateDispatch(*item, vaultResult->TargetState))
				return std::nullopt;

			return vaultResult;
		}

		// Vault to crouch up three steps.
		vaultResult = TestLaraVault3StepsToCrouch(item, coll);
		if (vaultResult.has_value() && settings.CrawlExtended)
		{
			vaultResult->TargetState = LS_VAULT_3_STEPS_CROUCH;
			if (!TestStateDispatch(*item, vaultResult->TargetState))
				return std::nullopt;

			return vaultResult;
		}

		// Auto jump to ledge.
		vaultResult = TestLaraLedgeAutoJump(item, coll);
		if (vaultResult.has_value())
		{
			vaultResult->TargetState = LS_AUTO_JUMP;
			if (!TestStateDispatch(*item, vaultResult->TargetState))
				return std::nullopt;

			return vaultResult;
		}
	}

	// TODO: Move ladder checks here when ladders are less prone to breaking.
	// In this case, they fail due to a reliance on ShiftItem(). @Sezz 2021.02.05

	// Auto jump to monkey swing.
	vaultResult = TestLaraAutoMonkeySwingJump(item, coll);
	if (vaultResult.has_value() && g_Configuration.EnableAutoMonkeySwingJump)
	{
		vaultResult->TargetState = LS_AUTO_JUMP;
		if (!TestStateDispatch(*item, vaultResult->TargetState))
			return std::nullopt;

		return vaultResult;
	}

	return std::nullopt;
}

// Temporary solution to ladder mounts until ladders stop breaking whenever anyone tries to do anything with them. @Sezz 2022.02.05
bool TestAndDoLaraLadderClimb(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (!IsHeld(In::Action) || !IsHeld(In::Forward) || lara->Control.HandStatus != HandStatus::Free)
		return false;

	if (TestEnvironment(ENV_FLAG_SWAMP, item) && lara->Context.WaterSurfaceDist < -CLICK(3))
		return false;

	// Auto jump to ladder.
	auto vaultResult = TestLaraLadderAutoJump(item, coll);
	if (vaultResult.has_value())
	{
		// TODO: Somehow harmonise Context.CalcJumpVelocity to work for both ledge and ladder auto jumps, because otherwise there will be a need for an odd workaround in the future.
		lara->Context.CalcJumpVelocity = -3 - sqrt(-9600 - 12 * std::max((vaultResult->Height - item->Pose.Position.y + CLICK(0.2f)), -CLICK(7.1f)));
		item->Animation.AnimNumber = LA_STAND_SOLID;
		item->Animation.FrameNumber = 0;
		item->Animation.TargetState = LS_JUMP_UP;
		item->Animation.ActiveState = LS_IDLE;
		lara->Control.TurnRate = 0;

		ShiftItem(item, coll);
		SnapItemToGrid(item, coll); // HACK: until fragile ladder code is refactored, we must exactly snap to grid.
		lara->Context.TargetOrientation = EulerAngles(0, item->Pose.Orientation.y, 0);
		AnimateItem(*item);

		return true;
	}

	// Mount ladder.
	vaultResult = TestLaraLadderMount(item, coll);
	if (vaultResult.has_value() && TestLaraClimbIdle(item, coll))
	{
		item->Animation.AnimNumber = LA_STAND_SOLID;
		item->Animation.FrameNumber = 0;
		item->Animation.TargetState = LS_LADDER_IDLE;
		item->Animation.ActiveState = LS_IDLE;
		lara->Control.HandStatus = HandStatus::Busy;
		lara->Control.TurnRate = 0;

		ShiftItem(item, coll);
		SnapItemToGrid(item, coll); // HACK: until fragile ladder code is refactored, we must exactly snap to grid.
		AnimateItem(*item);

		return true;
	}

	return false;
}

CrawlVaultTestResult TestLaraCrawlVaultTolerance(ItemInfo* item, CollisionInfo* coll, CrawlVaultTestSetup testSetup)
{
	int y = item->Pose.Position.y;
	auto probeA = GetPointCollision(*item, item->Pose.Orientation.y, testSetup.CrossDist, -LARA_HEIGHT_CRAWL);	// Crossing.
	auto probeB = GetPointCollision(*item, item->Pose.Orientation.y, testSetup.DestDist, -LARA_HEIGHT_CRAWL);		// Approximate destination.
	auto probeMiddle = GetPointCollision(*item);

	bool isSlope = testSetup.CheckSlope ? probeB.IsSteepFloor() : false;
	bool isDeath = testSetup.CheckDeath ? probeB.GetSector().Flags.Death : false;

	// Discard walls.
	if (probeA.GetFloorHeight() == NO_HEIGHT || probeB.GetFloorHeight() == NO_HEIGHT)
		return CrawlVaultTestResult{ false };

	// Check for slope or death sector (if applicable).
	if (isSlope || isDeath)
		return CrawlVaultTestResult{ false };

	// Assess point/room collision.
	if ((probeA.GetFloorHeight() - y) <= testSetup.LowerFloorBound &&							// Within lower floor bound.
		(probeA.GetFloorHeight() - y) >= testSetup.UpperFloorBound &&							// Within upper floor bound.
		abs(probeA.GetCeilingHeight() - probeA.GetFloorHeight()) > testSetup.ClampMin &&		// Crossing clamp limit.
		abs(probeB.GetCeilingHeight() - probeB.GetFloorHeight()) > testSetup.ClampMin &&		// Destination clamp limit.
		abs(probeMiddle.GetCeilingHeight() - probeA.GetFloorHeight()) >= testSetup.GapMin &&	// Gap is optically permissive (going up).
		abs(probeA.GetCeilingHeight() - probeMiddle.GetFloorHeight()) >= testSetup.GapMin &&	// Gap is optically permissive (going down).
		abs(probeA.GetFloorHeight() - probeB.GetFloorHeight()) <= testSetup.FloorBound &&		// Crossing/destination floor height difference suggests continuous crawl surface.
		(probeA.GetCeilingHeight() - y) < -testSetup.GapMin)									// Ceiling height is permissive.
	{
		return CrawlVaultTestResult{ true };
	}

	return CrawlVaultTestResult{ false };
}

CrawlVaultTestResult TestLaraCrawlUpStep(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: [-CLICK(1), -STEPUP_HEIGHT]

	CrawlVaultTestSetup testSetup
	{
		int(-CLICK(1)), -STEPUP_HEIGHT,
		LARA_HEIGHT_CRAWL,
		int(CLICK(0.6f)),
		int(CLICK(1.2f)),
		int(CLICK(2)),
		int(CLICK(1) - 1)
	};

	return TestLaraCrawlVaultTolerance(item, coll, testSetup);
}

CrawlVaultTestResult TestLaraCrawlDownStep(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: [STEPUP_HEIGHT, CLICK(1)]

	CrawlVaultTestSetup testSetup
	{
		STEPUP_HEIGHT, CLICK(1),
		LARA_HEIGHT_CRAWL,
		int(CLICK(0.6f)),
		int(CLICK(1.2f)),
		int(CLICK(2)),
		int(CLICK(1) - 1)
	};

	return TestLaraCrawlVaultTolerance(item, coll, testSetup);
}

CrawlVaultTestResult TestLaraCrawlExitDownStep(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: [STEPUP_HEIGHT, CLICK(1)]

	CrawlVaultTestSetup testSetup
	{
		STEPUP_HEIGHT, int(CLICK(1)),
		LARA_HEIGHT,
		int(CLICK(1.25f)),
		int(CLICK(1.2f)),
		int(CLICK(1.5f)),
		-MAX_HEIGHT,
		false, false
	};

	return TestLaraCrawlVaultTolerance(item, coll, testSetup);
}

CrawlVaultTestResult TestLaraCrawlExitJump(ItemInfo* item, CollisionInfo* coll)
{
	// Floor range: [NO_LOWER_BOUND, STEPUP_HEIGHT)

	CrawlVaultTestSetup testSetup
	{
		NO_LOWER_BOUND, STEPUP_HEIGHT + 1,
		LARA_HEIGHT,
		int(CLICK(1.25f)),
		int(CLICK(1.2f)),
		int(CLICK(1.5f)),
		NO_LOWER_BOUND,
		false, false
	};

	return TestLaraCrawlVaultTolerance(item, coll, testSetup);
}

CrawlVaultTestResult TestLaraCrawlVault(ItemInfo* item, CollisionInfo* coll)
{
	if (!(IsHeld(In::Action) || IsHeld(In::Jump)))
		return CrawlVaultTestResult{ false };

	// Crawl vault exit down 1 step.
	auto crawlVaultResult = TestLaraCrawlExitDownStep(item, coll);
	if (crawlVaultResult.Success)
	{
		if (IsHeld(In::Crouch) && TestLaraCrawlDownStep(item, coll).Success)
			crawlVaultResult.TargetState = LS_CRAWL_STEP_DOWN;
		else
			crawlVaultResult.TargetState = LS_CRAWL_EXIT_STEP_DOWN;

		crawlVaultResult.Success = TestStateDispatch(*item, crawlVaultResult.TargetState);
		return crawlVaultResult;
	}

	// Crawl vault exit jump.
	crawlVaultResult = TestLaraCrawlExitJump(item, coll);
	if (crawlVaultResult.Success)
	{
		if (IsHeld(In::Walk))
			crawlVaultResult.TargetState = LS_CRAWL_EXIT_FLIP;
		else
			crawlVaultResult.TargetState = LS_CRAWL_EXIT_JUMP;

		crawlVaultResult.Success = TestStateDispatch(*item, crawlVaultResult.TargetState);
		return crawlVaultResult;
	}

	// Crawl vault up 1 step.
	crawlVaultResult = TestLaraCrawlUpStep(item, coll);
	if (crawlVaultResult.Success)
	{
		crawlVaultResult.TargetState = LS_CRAWL_STEP_UP;
		crawlVaultResult.Success = TestStateDispatch(*item, crawlVaultResult.TargetState);
		return crawlVaultResult;
	}

	// Crawl vault down 1 step.
	crawlVaultResult = TestLaraCrawlDownStep(item, coll);
	if (crawlVaultResult.Success)
	{
		crawlVaultResult.TargetState = LS_CRAWL_STEP_DOWN;
		crawlVaultResult.Success = TestStateDispatch(*item, crawlVaultResult.TargetState);
		return crawlVaultResult;
	}

	return CrawlVaultTestResult{ false };
}

bool TestLaraCrawlToHang(ItemInfo* item, CollisionInfo* coll)
{
	int y = item->Pose.Position.y;
	int distance = CLICK(1.2f);
	auto probe = GetPointCollision(*item, item->Pose.Orientation.y + ANGLE(180.0f), distance, -LARA_HEIGHT_CRAWL);

	bool objectCollided = TestLaraObjectCollision(item, item->Pose.Orientation.y + ANGLE(180.0f), CLICK(1.2f), -LARA_HEIGHT_CRAWL);

	if (!objectCollided &&										// No obstruction.
		(probe.GetFloorHeight() - y) >= LARA_HEIGHT_STRETCH &&	// Highest floor bound.
		(probe.GetCeilingHeight() - y) <= -CLICK(0.75f) &&		// Gap is optically permissive.
		probe.GetFloorHeight() != NO_HEIGHT)
	{
		return true;
	}

	return false;
}

bool TestLaraPoleCollision(ItemInfo* item, CollisionInfo* coll, bool goingUp, float offset)
{
	static constexpr auto poleProbeCollRadius = 16.0f;

	bool atLeastOnePoleCollided = false;

	auto collObjects = GetCollidedObjects(*item, true, false, BLOCK(2), ObjectCollectionMode::Items);
	if (!collObjects.IsEmpty())
	{
		auto laraBox = GameBoundingBox(item).ToBoundingOrientedBox(item->Pose);

		// HACK: Because Core implemented upward pole movement as a SetPosition command, we can't precisely
		// check her position. So we add a fixed height offset.

		// Offset a sphere when jumping toward pole.
		auto sphereOffset2D = Vector3::Zero;
		sphereOffset2D = Geometry::TranslatePoint(sphereOffset2D, item->Pose.Orientation.y, coll->Setup.Radius + item->Animation.Velocity.z);

		auto spherePos = laraBox.Center + Vector3(0.0f, (laraBox.Extents.y + poleProbeCollRadius + offset) * (goingUp ? -1 : 1), 0.0f);

		auto sphere = BoundingSphere(spherePos, poleProbeCollRadius);
		auto offsetSphere = BoundingSphere(spherePos + sphereOffset2D, poleProbeCollRadius);

		//DrawDebugSphere(sphere.Center, 16.0f, Vector4(1, 0, 0, 1), RendererDebugPage::CollisionStats);

		for (const auto* itemPtr : collObjects.Items)
		{
			if (itemPtr->ObjectNumber != ID_POLEROPE)
				continue;

			auto poleBox = GameBoundingBox(itemPtr).ToBoundingOrientedBox(itemPtr->Pose);
			poleBox.Extents = poleBox.Extents + Vector3(coll->Setup.Radius, 0.0f, coll->Setup.Radius);

			//DrawDebugBox(poleBox, Vector4(0, 0, 1, 1), RendererDebugPage::CollisionStats);

			if (poleBox.Intersects(sphere) || poleBox.Intersects(offsetSphere))
			{
				atLeastOnePoleCollided = true;
				break;
			}
		}
	}

	return atLeastOnePoleCollided;
}

bool TestLaraPoleUp(ItemInfo* item, CollisionInfo* coll)
{
	if (!TestLaraPoleCollision(item, coll, true, CLICK(1)))
		return false;

	return (coll->Middle.Ceiling < -CLICK(1));
}

bool TestLaraPoleDown(ItemInfo* item, CollisionInfo* coll)
{
	if (!TestLaraPoleCollision(item, coll, false))
		return false;

	return (coll->Middle.Floor > 0);
}
