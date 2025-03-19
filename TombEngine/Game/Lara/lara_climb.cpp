#include "framework.h"
#include "Game/Lara/lara_climb.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_overhang.h"
#include "Game/Lara/lara_tests.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Input;

constexpr auto LADDER_TEST_MARGIN = 8;
constexpr auto LADDER_TEST_DISTANCE = CLICK(0.5f) - LADDER_TEST_MARGIN;
constexpr auto LADDER_CLIMB_SHIFT = 70;

// -----------------------------
// WALL CLIMB
// Control & Collision Functions
// -----------------------------

void lara_col_climb_end(ItemInfo* item, CollisionInfo* coll)
{
	return;
}

void lara_as_climb_end(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.flags = CF_FOLLOW_CENTER;
	Camera.targetAngle = -ANGLE(45.0f);
}

void lara_col_climb_down(ItemInfo* item, CollisionInfo* coll)
{
	if (LaraCheckForLetGo(item, coll) || item->Animation.AnimNumber != LA_LADDER_DOWN)
		return;

	int frame = item->Animation.FrameNumber;
	int yShift = 0;

	switch (frame)
	{
	case 0:
		yShift = 0;
		break;

	case 28:
	case 29:
		yShift = CLICK(1);
		break;

	case 57:
		yShift = CLICK(2);
		break;

	default:
		return;
	}

	item->Pose.Position.y += yShift + CLICK(1);

	int shiftLeft = 0;
	int shiftRight = 0;
	int resultRight = LaraTestClimbPos(item, coll->Setup.Radius, coll->Setup.Radius + LADDER_TEST_DISTANCE, -CLICK(2), CLICK(2), &shiftRight);
	int resultLeft = LaraTestClimbPos(item, coll->Setup.Radius, -(coll->Setup.Radius + LADDER_TEST_DISTANCE), -CLICK(2), CLICK(2), &shiftLeft);

	item->Pose.Position.y -= CLICK(1);

	if (IsHeld(In::Back) &&
		resultRight != 0 && resultLeft != 0 &&
		resultRight != -2 && resultLeft != -2)
	{
		if (shiftRight && shiftLeft)
		{
			if (shiftRight < 0 != shiftLeft < 0)
			{
				item->Animation.TargetState = LS_LADDER_IDLE;
				AnimateItem(*item);
				return;
			}

			if (shiftRight < 0 && shiftRight < shiftLeft ||
				shiftRight > 0 && shiftRight > shiftLeft)
			{
				shiftLeft = shiftRight;
			}
		}

		if (resultRight == -1 || resultLeft == -1)
		{
			SetAnimation(*item, LA_LADDER_IDLE);
			item->Animation.TargetState = LS_HANG;

			AnimateItem(*item);
		}
		else
		{
			item->Animation.TargetState = LS_LADDER_DOWN;
			item->Pose.Position.y -= yShift;
		}

		return;
	}

	item->Animation.TargetState = LS_LADDER_IDLE;

	if (yShift != 0)
		AnimateItem(*item);
}

void lara_as_climb_down(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetElevation = -ANGLE(45.0f);

	// Overhang hook.
	SlopeClimbDownExtra(item, coll);
}

void lara_col_climb_up(ItemInfo* item, CollisionInfo* coll)
{
	if (!LaraCheckForLetGo(item, coll) && item->Animation.AnimNumber == LA_LADDER_UP)
	{
		int frame = item->Animation.FrameNumber;
		int yShift;
		int resultRight, resultLeft;
		int shiftRight, shiftLeft;
		int ledgeRight, ledgeLeft;

		if (frame == 0)
			yShift = 0;
		else if (frame == 28 || frame == 29)
			yShift = -CLICK(1);
		else if (frame == 57)
			yShift = -CLICK(2);
		else
			return;

		item->Pose.Position.y += yShift - CLICK(1);

		resultRight = LaraTestClimbUpPos(item, coll->Setup.Radius, coll->Setup.Radius + LADDER_TEST_DISTANCE, &shiftRight, &ledgeRight);
		resultLeft = LaraTestClimbUpPos(item, coll->Setup.Radius, -(coll->Setup.Radius + LADDER_TEST_DISTANCE), &shiftLeft, &ledgeLeft);

		item->Pose.Position.y += CLICK(1);
		 
		if (IsHeld(In::Forward) && resultRight && resultLeft)
		{
			if (resultRight < 0 || resultLeft < 0)
			{
				item->Animation.TargetState = LS_LADDER_IDLE;

				AnimateItem(*item);

				if (abs(ledgeRight - ledgeLeft) <= LADDER_TEST_DISTANCE)
				{
					if (resultRight != -1 || resultLeft != -1)
					{
						item->Animation.TargetState = LS_LADDER_TO_CROUCH;
						item->Animation.RequiredState = LS_CROUCH_IDLE;
					}
					else
					{
						item->Animation.TargetState = LS_GRABBING;
						item->Pose.Position.y += (ledgeRight + ledgeLeft) / 2 - CLICK(1);
					}
				}
			}
			else
			{
				item->Animation.TargetState = LS_LADDER_UP;
				item->Pose.Position.y -= yShift;
			}
		}
		else
		{
			item->Animation.TargetState = LS_LADDER_IDLE;

			if (yShift != 0)
				AnimateItem(*item);
		}
	}
}

void lara_as_climb_up(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetElevation = ANGLE(30.0f);
}

void lara_col_climb_right(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (!LaraCheckForLetGo(item, coll))
	{
		int shift = 0;
		lara->Control.MoveAngle = item->Pose.Orientation.y + ANGLE(90.0f);
		LaraDoClimbLeftRight(item, coll, LaraTestClimbPos(item, coll->Setup.Radius, coll->Setup.Radius + LADDER_TEST_DISTANCE, -CLICK(2), CLICK(2), &shift), shift);
	}
}

void lara_as_climb_right(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = ANGLE(30.0f);
	Camera.targetElevation = -ANGLE(15.0f);

	if (!(IsHeld(In::Right) || IsHeld(In::StepRight)))
		item->Animation.TargetState = LS_LADDER_IDLE;
}

void lara_col_climb_left(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (!LaraCheckForLetGo(item, coll))
	{
		int shift = 0;
		lara->Control.MoveAngle = item->Pose.Orientation.y - ANGLE(90.0f);
		LaraDoClimbLeftRight(item, coll, LaraTestClimbPos(item, coll->Setup.Radius, -(coll->Setup.Radius + LADDER_TEST_DISTANCE), -CLICK(2), CLICK(2), &shift), shift);
	}
}

void lara_as_climb_left(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = -ANGLE(30.0f);
	Camera.targetElevation = -ANGLE(15.0f);

	if (!(IsHeld(In::Left) || IsHeld(In::StepLeft)))
		item->Animation.TargetState = LS_LADDER_IDLE;
}

void lara_col_climb_idle(ItemInfo* item, CollisionInfo* coll)
{
	int yShift;
	int resultRight, resultLeft;
	int ledgeRight, ledgeLeft;

	if (LaraCheckForLetGo(item, coll) || item->Animation.AnimNumber != LA_LADDER_IDLE)
		return;

	if (!IsHeld(In::Forward))
	{
		if (!IsHeld(In::Back))
			return;

		if (item->Animation.TargetState == LS_HANG)
			return;

		item->Animation.TargetState = LS_LADDER_IDLE;
		item->Pose.Position.y += CLICK(1);
		
		resultRight = LaraTestClimbPos(item, coll->Setup.Radius, coll->Setup.Radius + LADDER_TEST_DISTANCE, -CLICK(2), CLICK(2), &ledgeRight);
		resultLeft = LaraTestClimbPos(item, coll->Setup.Radius, -LADDER_TEST_DISTANCE - coll->Setup.Radius, -CLICK(2), CLICK(2), &ledgeLeft);
		
		item->Pose.Position.y -= CLICK(1);
		
		if (!resultRight || !resultLeft || resultLeft == -2 || resultRight == -2)
			return;

		yShift = ledgeLeft;

		if (ledgeRight && ledgeLeft)
		{
			if (ledgeLeft < 0 != ledgeRight < 0)
				return;
			if (ledgeRight < 0 == ledgeRight < ledgeLeft)
				yShift = ledgeRight;
		}

		if (resultRight == 1 && resultLeft == 1)
		{
			item->Animation.TargetState = LS_LADDER_DOWN;
			item->Pose.Position.y += yShift;
		}
		else
			item->Animation.TargetState = LS_HANG;
	}
	else if (item->Animation.TargetState != LS_GRABBING)
	{
		int shiftRight, shiftLeft;

		item->Animation.TargetState = LS_LADDER_IDLE;
		resultRight = LaraTestClimbUpPos(item, coll->Setup.Radius, coll->Setup.Radius + LADDER_TEST_DISTANCE, &shiftRight, &ledgeRight);
		resultLeft = LaraTestClimbUpPos(item, coll->Setup.Radius, -LADDER_TEST_DISTANCE - coll->Setup.Radius, &shiftLeft, &ledgeLeft);

		// Overhang + ladder-to-monkey hook.
		if (!resultRight || !resultLeft)
		{
			if (LadderMonkeyExtra(item, coll))
				return;
		}

		// Added check to avoid climbing through bridges.
		if (resultRight == 0 && resultLeft == 0)
			return;

		if (resultRight >= 0 && resultLeft >= 0)
		{
			yShift = shiftLeft;

			if (shiftRight)
			{
				if (shiftLeft)
				{
					if (shiftLeft < 0 != shiftRight < 0)
						return;
					if (shiftRight < 0 == shiftRight < shiftLeft)
						yShift = shiftRight;
				}
				else
					yShift = shiftRight;
			}

			// HACK: Prevent climbing inside sloped ceilings. Breaks overhang even more, but that shouldn't matter since we'll be doing it over. -- Sezz 2022.05.13
			int y = item->Pose.Position.y - (coll->Setup.Height + CLICK(0.5f));
			auto probe = GetPointCollision(*item, 0, 0, -(coll->Setup.Height + CLICK(0.5f)));
			if ((probe.GetCeilingHeight() - y) < 0)
			{
				item->Animation.TargetState = LS_LADDER_UP;
				item->Pose.Position.y += yShift;
			}
		}
		else if (abs(ledgeLeft - ledgeRight) <= LADDER_TEST_DISTANCE)
		{
			if (resultRight == -1 && resultLeft == -1)
			{
				item->Animation.TargetState = LS_GRABBING;
				item->Pose.Position.y += (ledgeRight + ledgeLeft) / 2 - CLICK(1);
			}
			else
			{
				item->Animation.TargetState = LS_LADDER_TO_CROUCH;
				item->Animation.RequiredState = LS_CROUCH_IDLE;
			}
		}
	}
}

void lara_as_climb_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Free;
	lara->Control.IsClimbingLadder = true;
	coll->Setup.EnableSpasm = false;
	coll->Setup.EnableObjectPush = false;
	Camera.targetElevation = -ANGLE(20.0f);

	if (item->Animation.AnimNumber == LA_LADDER_DISMOUNT_LEFT_START)
		Camera.targetAngle = -ANGLE(60.0f);

	if (item->Animation.AnimNumber == LA_LADDER_DISMOUNT_RIGHT_START)
		Camera.targetAngle = ANGLE(60.0f);

	if (IsHeld(In::Left) || IsHeld(In::StepLeft))
	{
		item->Animation.TargetState = LS_LADDER_LEFT;
		lara->Control.MoveAngle = item->Pose.Orientation.y - ANGLE(90.0f);
	}
	else if (IsHeld(In::Right) || IsHeld(In::StepRight))
	{
		item->Animation.TargetState = LS_LADDER_RIGHT;
		lara->Control.MoveAngle = item->Pose.Orientation.y + ANGLE(90.0f);
	}
	else if (IsHeld(In::Jump))
	{
		if (item->Animation.AnimNumber == LA_LADDER_IDLE)
		{
			item->Animation.TargetState = LS_JUMP_BACK;
			lara->Control.HandStatus = HandStatus::Free;
			lara->Control.MoveAngle = item->Pose.Orientation.y + ANGLE(180.0f);
		}
	}

	// Overhang hook.
	SlopeClimbExtra(item, coll);
}

void lara_as_climb_stepoff_left(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = -ANGLE(60.0f);
	Camera.targetElevation = -ANGLE(15.0f);

	item->Pose.Orientation.y -= ANGLE(90.0f);
}

void lara_as_climb_stepoff_right(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = ANGLE(60.0f);
	Camera.targetElevation = -ANGLE(15.0f);

	item->Pose.Orientation.y += ANGLE(90.0f);
}

// --------
// HELPERS:
// --------

short GetClimbFlags(int x, int y, int z, short roomNumber)
{
	return GetClimbFlags(&GetPointCollision(Vector3i(x, y, z), roomNumber).GetBottomSector());
}

short GetClimbFlags(FloorInfo* floor)
{
	short result = 0;

	if (floor->Flags.ClimbEast)
		result |= (short)ClimbDirectionFlags::East;

	if (floor->Flags.ClimbWest)
		result |= (short)ClimbDirectionFlags::West;

	if (floor->Flags.ClimbNorth)
		result |= (short)ClimbDirectionFlags::North;

	if (floor->Flags.ClimbSouth)
		result |= (short)ClimbDirectionFlags::South;

	return result;
}

ClimbDirectionFlags GetClimbDirectionFlags(short angle)
{
	switch (GetQuadrant(angle))
	{
	default:
	case NORTH:
		return ClimbDirectionFlags::North;

	case EAST:
		return ClimbDirectionFlags::East;

	case SOUTH:
		return ClimbDirectionFlags::South;

	case WEST:
		return ClimbDirectionFlags::West;
	}
}

int LaraTestClimbPos(ItemInfo* item, int front, int right, int origin, int height, int* shift)
{
	int x;
	int z;
	int xFront = 0;
	int zFront = 0;

	switch (GetQuadrant(item->Pose.Orientation.y))
	{
	case NORTH:
		x = right;
		z = front;
		zFront = CLICK(1);
		break;

	case EAST:
		x = front;
		z = -right;
		xFront = CLICK(1);
		break;

	case SOUTH:
		x = -right;
		z = -front;
		zFront = -CLICK(1);
		break;

	case WEST:
	default:
		x = -front;
		z = right;
		xFront = -CLICK(1);
		break;
	}

	return LaraTestClimb(item, x, origin, z, xFront, zFront, height, shift);
}

void LaraDoClimbLeftRight(ItemInfo* item, CollisionInfo* coll, int result, int shift)
{
	if (result == 1)
	{
		if (IsHeld(In::Left))
			item->Animation.TargetState = LS_LADDER_LEFT;
		else if (IsHeld(In::Right))
			item->Animation.TargetState = LS_LADDER_RIGHT;
		else
			item->Animation.TargetState = LS_LADDER_IDLE;

		item->Pose.Position.y += shift;
		return;
	}

	if (result != 0)
	{
		item->Animation.TargetState = LS_HANG;

		do
		{
			AnimateItem(*item);
		} while (item->Animation.ActiveState != LS_HANG);

		item->Pose.Position.x = coll->Setup.PrevPosition.x;
		item->Pose.Position.z = coll->Setup.PrevPosition.z;

		return;
	}

	item->Pose.Position.x = coll->Setup.PrevPosition.x;
	item->Pose.Position.z = coll->Setup.PrevPosition.z;

	item->Animation.TargetState = LS_LADDER_IDLE;
	item->Animation.ActiveState = LS_LADDER_IDLE;

	if (coll->Setup.PrevState != LS_LADDER_IDLE)
	{	
		SetAnimation(*item, LA_LADDER_IDLE);
		return;
	}

	if (IsHeld(In::Left))
	{
		short troomnumber = item->RoomNumber;
		int dx = int(sin(TO_RAD(item->Pose.Orientation.y - ANGLE(90.0f))) * 10);
		int dz = int(cos(TO_RAD(item->Pose.Orientation.y - ANGLE(90.0f))) * 10);
		int height = GetFloorHeight(GetFloor(item->Pose.Position.x + dx, item->Pose.Position.y, item->Pose.Position.z + dz, &troomnumber),
			item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z) - item->Pose.Position.y;
		if (height < CLICK(1.5f)) // LADDER dismounts (left/right)
		{
			item->Animation.TargetState = LS_LADDER_DISMOUNT_LEFT;
			item->Animation.ActiveState = LS_MISC_CONTROL;
		}
	}
	else if (IsHeld(In::Right))
	{
		short troomnumber = item->RoomNumber;
		int dx = int(sin(TO_RAD(item->Pose.Orientation.y + ANGLE(90.0f))) * 10);
		int dz = int(cos(TO_RAD(item->Pose.Orientation.y + ANGLE(90.0f))) * 10);
		int height = GetFloorHeight(GetFloor(item->Pose.Position.x + dx, item->Pose.Position.y, item->Pose.Position.z + dz, &troomnumber),
			item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z) - item->Pose.Position.y;

		if (height < CLICK(1.5f)) // LADDER dismounts (left/right)
		{
			item->Animation.TargetState = LS_LADDER_DISMOUNT_RIGHT;
			item->Animation.ActiveState = LS_MISC_CONTROL;
		}
	}

	if (IsHeld(In::Left))
	{
		int flag = LaraClimbLeftCornerTest(item, coll);

		if (flag)
		{
			if (flag <= 0)
				SetAnimation(*item, LA_LADDER_LEFT_CORNER_INNER_START);
			else
				SetAnimation(*item, LA_LADDER_LEFT_CORNER_OUTER_START);

			return;
		}
	}
	else if (IsHeld(In::Right))
	{
		int flag = LaraClimbRightCornerTest(item, coll);

		if (flag)
		{
			if (flag <= 0)
				SetAnimation(*item, LA_LADDER_RIGHT_CORNER_INNER_START);
			else
				SetAnimation(*item, LA_LADDER_RIGHT_CORNER_OUTER_START);

			return;
		}
	}

	item->Animation.AnimNumber = coll->Setup.PrevAnimNumber;
	item->Animation.FrameNumber = coll->Setup.PrevFrameNumber;

	AnimateItem(*item);
}

int LaraClimbRightCornerTest(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	int result = 0;

	if (item->Animation.AnimNumber != LA_LADDER_RIGHT)
		return 0;

	auto oldPos = item->Pose;
	auto oldRot = lara->Control.MoveAngle;

	short angle = GetQuadrant(item->Pose.Orientation.y);
	int x, z;

	if (angle && angle != SOUTH)
	{
		x = (item->Pose.Position.x & -BLOCK(1)) - (item->Pose.Position.z % BLOCK(1)) + BLOCK(1);
		z = (item->Pose.Position.z & -BLOCK(1)) - (item->Pose.Position.x % BLOCK(1)) + BLOCK(1);
	}
	else
	{
		x = item->Pose.Position.x ^ (item->Pose.Position.x ^ item->Pose.Position.z) & WALL_MASK;
		z = item->Pose.Position.z ^ (item->Pose.Position.x ^ item->Pose.Position.z) & WALL_MASK;
	}

	int shift = 0;

	if (GetClimbFlags(x, item->Pose.Position.y, z, item->RoomNumber) & (short)LeftExtRightIntTab[angle])
	{
		lara->Context.NextCornerPos.Position.x = item->Pose.Position.x = x;
		lara->Context.NextCornerPos.Position.y = item->Pose.Position.y;
		lara->Context.NextCornerPos.Position.z = item->Pose.Position.z = z;
		lara->Context.NextCornerPos.Orientation.y = item->Pose.Orientation.y = lara->Control.MoveAngle = item->Pose.Orientation.y + ANGLE(90);

		result = LaraTestClimbPos(item, coll->Setup.Radius, coll->Setup.Radius + LADDER_TEST_DISTANCE, -CLICK(2), CLICK(2), &shift);
		item->ItemFlags[3] = result;
	}

	if (!result)
	{
		item->Pose = oldPos;
		lara->Control.MoveAngle = oldRot;

		switch (angle)
		{
		case NORTH:
			x = ((item->Pose.Position.x + BLOCK(1)) & -BLOCK(1)) - (item->Pose.Position.z % BLOCK(1)) + BLOCK(1);
			z = ((item->Pose.Position.z + BLOCK(1)) & -BLOCK(1)) - (item->Pose.Position.x % BLOCK(1)) + BLOCK(1);
			break;

		case SOUTH:
			x = ((item->Pose.Position.x - BLOCK(1)) & -BLOCK(1)) - (item->Pose.Position.z % BLOCK(1)) + BLOCK(1);
			z = ((item->Pose.Position.z - BLOCK(1)) & -BLOCK(1)) - (item->Pose.Position.x % BLOCK(1)) + BLOCK(1);
			break;

		case EAST:
			x = ((item->Pose.Position.z ^ item->Pose.Position.x) % BLOCK(1)) ^ (item->Pose.Position.x + BLOCK(1));
			z = (item->Pose.Position.z ^ ((item->Pose.Position.z ^ item->Pose.Position.x) % BLOCK(1))) - BLOCK(1);
			break;

		case WEST:
		default:
			x = (item->Pose.Position.x ^ (item->Pose.Position.z ^ item->Pose.Position.x) % BLOCK(1)) - BLOCK(1);
			z = ((item->Pose.Position.z ^ item->Pose.Position.x) % BLOCK(1)) ^ (item->Pose.Position.z + BLOCK(1));
			break;

		}

		if (GetClimbFlags(x, item->Pose.Position.y, z, item->RoomNumber) & (short)LeftIntRightExtTab[angle])
		{
			lara->Context.NextCornerPos.Position.x = item->Pose.Position.x = x;
			lara->Context.NextCornerPos.Position.y = item->Pose.Position.y;
			lara->Context.NextCornerPos.Position.z = item->Pose.Position.z = z;
			lara->Context.NextCornerPos.Orientation.y = item->Pose.Orientation.y = lara->Control.MoveAngle = item->Pose.Orientation.y - ANGLE(90.0f);

			result = LaraTestClimbPos(item, coll->Setup.Radius, coll->Setup.Radius + LADDER_TEST_DISTANCE, -CLICK(2), CLICK(2), &shift);
			item->ItemFlags[3] = result;
		}
	}
	else
		result = -1;

	item->Pose = oldPos;
	lara->Control.MoveAngle = oldRot;

	return result;
}

int LaraClimbLeftCornerTest(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	int result = 0;

	if (item->Animation.AnimNumber != LA_LADDER_LEFT)
		return 0;

	auto oldPos = item->Pose;
	auto oldRot = lara->Control.MoveAngle;

	short angle = GetQuadrant(item->Pose.Orientation.y);
	int x, z;

	if (angle && angle != SOUTH)
	{
		x = item->Pose.Position.x ^ (item->Pose.Position.x ^ item->Pose.Position.z) & WALL_MASK;
		z = item->Pose.Position.z ^ (item->Pose.Position.x ^ item->Pose.Position.z) & WALL_MASK;
	}
	else
	{
		x = (item->Pose.Position.x & -BLOCK(1)) - (item->Pose.Position.z & WALL_MASK) + BLOCK(1);
		z = (item->Pose.Position.z & -BLOCK(1)) - (item->Pose.Position.x & WALL_MASK) + BLOCK(1);
	}

	int shift = 0;

	if (GetClimbFlags(x, item->Pose.Position.y, z, item->RoomNumber) & (short)LeftIntRightExtTab[angle])
	{
		lara->Context.NextCornerPos.Position.x = item->Pose.Position.x = x;
		lara->Context.NextCornerPos.Position.y = item->Pose.Position.y;
		lara->Context.NextCornerPos.Position.z = item->Pose.Position.z = z;
		lara->Context.NextCornerPos.Orientation.y = item->Pose.Orientation.y = lara->Control.MoveAngle = item->Pose.Orientation.y - ANGLE(90.0f);

		result = LaraTestClimbPos(item, coll->Setup.Radius, -coll->Setup.Radius - LADDER_TEST_DISTANCE, -CLICK(2), CLICK(2), &shift);
		item->ItemFlags[3] = result;
	}

	if (!result)
	{
		item->Pose = oldPos;
		lara->Control.MoveAngle = oldRot;

		switch (angle)
		{
		case NORTH:
			x = (item->Pose.Position.x ^ ((item->Pose.Position.z ^ item->Pose.Position.x) & WALL_MASK)) - BLOCK(1);
			z = ((item->Pose.Position.z ^ item->Pose.Position.x) & WALL_MASK) ^ (item->Pose.Position.z + BLOCK(1));
			break;

		case SOUTH:
			x = ((item->Pose.Position.z ^ item->Pose.Position.x) & WALL_MASK) ^ (item->Pose.Position.x + BLOCK(1));
			z = ((item->Pose.Position.z ^ item->Pose.Position.x) & WALL_MASK) ^ (item->Pose.Position.z - BLOCK(1));
			break;

		case EAST:
			x = ((item->Pose.Position.x + BLOCK(1)) & -BLOCK(1)) - (item->Pose.Position.z & WALL_MASK) + BLOCK(1);
			z = ((item->Pose.Position.z + BLOCK(1)) & -BLOCK(1)) - (item->Pose.Position.x & WALL_MASK) + BLOCK(1);
			break;

		case WEST:
		default:
			x = (item->Pose.Position.x & -BLOCK(1)) - (item->Pose.Position.z & WALL_MASK);
			z = (item->Pose.Position.z & -BLOCK(1)) - (item->Pose.Position.x & WALL_MASK);
			break;
		}

		if (GetClimbFlags(x, item->Pose.Position.y, z, item->RoomNumber) & (short)LeftExtRightIntTab[angle])
		{
			lara->Context.NextCornerPos.Position.x = item->Pose.Position.x = x;
			lara->Context.NextCornerPos.Position.y = item->Pose.Position.y;
			lara->Context.NextCornerPos.Position.z = item->Pose.Position.z = z;
			lara->Context.NextCornerPos.Orientation.y = item->Pose.Orientation.y = lara->Control.MoveAngle = item->Pose.Orientation.y + ANGLE(90.0f);

			item->ItemFlags[3] = LaraTestClimbPos(item, coll->Setup.Radius, -coll->Setup.Radius - LADDER_TEST_DISTANCE, -CLICK(2), CLICK(2), &shift);
			result = item->ItemFlags[3] != 0;
		}
	}
	else
		result = -1;

	item->Pose = oldPos;
	lara->Control.MoveAngle = oldRot;
	return result;
}

int LaraTestClimb(ItemInfo* item, int xOffset, int yOffset, int zOffset, int xFront, int zFront, int itemHeight, int* shift)
{
	auto* lara = GetLaraInfo(item);

	*shift = 0;
	int hang = 1;

	int x = item->Pose.Position.x + xOffset;
	int y = item->Pose.Position.y + yOffset;
	int z = item->Pose.Position.z + zOffset;

	auto probeUp = GetPointCollision(Vector3i(x, y - CLICK(0.5f), z), item->RoomNumber);
	auto probeDown = GetPointCollision(Vector3i(x, y, z), item->RoomNumber);

	if (!lara->Control.CanClimbLadder && !TestLaraNearClimbableWall(item, &probeDown.GetBottomSector()))
		return 0;

	int height = probeUp.GetFloorHeight();

	if (height == NO_HEIGHT)
		return 0;

	height -= (CLICK(0.5f) + y + itemHeight);
	if (height < -LADDER_CLIMB_SHIFT)
		return 0;

	if (height < 0)
		*shift = height;

	int ceiling = probeDown.GetCeilingHeight() - y;
	if (ceiling > LADDER_CLIMB_SHIFT)
		return 0;

	if (ceiling > 0)
	{
		if (*shift)
			return 0;
		*shift = ceiling;
	}

	if ((itemHeight + height) < 900)
		hang = 0;

	int dz = zFront + z;
	int dx = xFront + x;

	auto probeFront = GetPointCollision(Vector3i(dx, y, dz), item->RoomNumber);
	height = probeFront.GetFloorHeight();
	
	if (height != NO_HEIGHT)
		height -= y;

	if (height <= LADDER_CLIMB_SHIFT)
	{
		if (height > 0)
		{
			if (*shift < 0)
				return 0;

			if (height > *shift)
				*shift = height;
		}

		auto probeTop = GetPointCollision(Vector3i(x, y + itemHeight, z), item->RoomNumber);
		auto probeTopFront = GetPointCollision(Vector3i(dx, y + itemHeight, dz), probeTop.GetRoomNumber());
		ceiling = probeTopFront.GetCeilingHeight();
		
		if (ceiling == NO_HEIGHT)
			return 1;

		if (ceiling - y <= height)
			return 1;

		if (ceiling - y >= CLICK(2))
			return 1;

		if (ceiling - y <= 442)
			return -(hang != 0);

		if (*shift > 0)
			return -(hang != 0);

		*shift = ceiling - y - CLICK(2);
		return 1;
	}
	
	ceiling = probeFront.GetCeilingHeight() - y;
	if (ceiling >= CLICK(2))
		return 1;

	if (ceiling > CLICK(2) - LADDER_CLIMB_SHIFT)
	{
		if (*shift > 0)
			return -(hang != 0);

		*shift = ceiling - CLICK(2);
		return 1;
	}

	if (ceiling > 0)
		return -(hang != 0);

	if (ceiling <= -LADDER_CLIMB_SHIFT || !hang || *shift > 0)
		return 0;

	if (*shift > ceiling)
		*shift = ceiling;

	return -1;
}

int LaraTestClimbUpPos(ItemInfo* item, int front, int right, int* shift, int* ledge)
{
	auto probePos = Geometry::TranslatePoint(item->Pose.Position, item->Pose.Orientation.y, front, -CLICK(3), right);
	auto probeOffset = Geometry::TranslatePoint(Vector3i::Zero, item->Pose.Orientation.y, 4.0f);

	*shift = 0;

	// Test center.
	auto pointColl = GetPointCollision(*item);
	int vPos = item->Pose.Position.y - CLICK(4);
	if ((pointColl.GetCeilingHeight() - vPos) > LADDER_CLIMB_SHIFT)
		return 0;

	pointColl = GetPointCollision(probePos, item->RoomNumber);
	int ceiling = (CLICK(1) - probePos.y) + pointColl.GetCeilingHeight();

	pointColl = GetPointCollision(Vector3i(probePos.x + probeOffset.x, probePos.y, probePos.z + probeOffset.z), pointColl.GetRoomNumber());
	int height = pointColl.GetFloorHeight();

	if (height == NO_HEIGHT)
	{
		*ledge = NO_HEIGHT;
	}
	else
	{
		height -= probePos.y;
		*ledge = height;
	}
	
	if (ceiling > LADDER_CLIMB_SHIFT)
		return 0;

	if (ceiling > 0)
		*shift = ceiling;
	
	if (height == NO_HEIGHT)
	{
		return 1;
	}
	else
	{
		if (height <= CLICK(0.5f))
		{
			if (height > 0 && height > *shift)
				*shift = height;

			pointColl = GetPointCollision(Vector3i(probePos.x, probePos.y + CLICK(2), probePos.z), item->RoomNumber);
			pointColl = GetPointCollision(Vector3i(probePos.x + probeOffset.x, probePos.y + CLICK(2), probePos.z + probeOffset.z), pointColl.GetRoomNumber());

			ceiling = pointColl.GetCeilingHeight() - probePos.y;
			if (ceiling <= height)
				return 1;

			if (ceiling >= CLICK(2))
				return 1;					
			else
				return 0;
		}
		else
		{
			ceiling = GetPointCollision(Vector3i(probePos.x + probeOffset.x, probePos.y, probePos.z + probeOffset.z), pointColl.GetRoomNumber()).GetCeilingHeight() - probePos.y;
			if (ceiling < CLICK(2))
			{
				if ((height - ceiling) <= LARA_HEIGHT)
				{
					if ((height - ceiling) < CLICK(2))
						return 0;

					*shift = height;
					return -2;
				}
				else
				{
					*shift = height;
					return -1;
				}
			}
			else
			{
				return 1;
			}
		}
	}

	return -2;
}

bool LaraCheckForLetGo(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	item->Animation.Velocity.y = 0;
	item->Animation.IsAirborne = false;

	if (IsHeld(In::Action) && item->HitPoints > 0 || item->Animation.AnimNumber == LA_ONWATER_TO_LADDER) // Can't let go on this anim
		return false;

	ResetPlayerFlex(item);

	SetAnimation(*item, LA_FALL_START);

	item->Animation.Velocity.z = 2;
	item->Animation.Velocity.y = 1;
	item->Animation.IsAirborne = true;
	lara->Control.HandStatus = HandStatus::Free;
	return true;
}
