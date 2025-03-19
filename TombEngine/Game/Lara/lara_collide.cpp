#include "framework.h"
#include "Game/Lara/lara_collide.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/PlayerContext.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_swim.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Setup.h"
#include "Objects/Sink.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Entities::Player;
using namespace TEN::Input;

constexpr auto DEFLECT_STRAIGHT_ANGLE		= ANGLE(5.0f);
constexpr auto DEFLECT_DIAGONAL_ANGLE		= ANGLE(12.0f);
constexpr auto DEFLECT_STRAIGHT_ANGLE_CRAWL = ANGLE(2.0f);
constexpr auto DEFLECT_DIAGONAL_ANGLE_CRAWL = ANGLE(5.0f);

// -----------------------------
// COLLISION TEST FUNCTIONS
// For State Control & Collision
// -----------------------------

bool LaraDeflectEdge(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (coll->CollisionType == CollisionType::Front || coll->CollisionType == CollisionType::TopFront)
	{
		ShiftItem(item, coll);

		item->Animation.TargetState = LS_IDLE;
		item->Animation.Velocity.z = 0;
		return true;
	}

	if (coll->CollisionType == CollisionType::Left)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y += coll->DiagonalStepAtLeft() ? DEFLECT_DIAGONAL_ANGLE : DEFLECT_STRAIGHT_ANGLE;
	}
	else if (coll->CollisionType == CollisionType::Right)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y -= coll->DiagonalStepAtRight() ? DEFLECT_DIAGONAL_ANGLE : DEFLECT_STRAIGHT_ANGLE;
	}
	else if (coll->LastBridgeItemNumber != NO_VALUE)
	{
		ShiftItem(item, coll);
	}

	return false;
}

bool LaraDeflectTopSide(ItemInfo* item, CollisionInfo* coll)
{
	// HACK: If we are falling down, collision is CollisionType::Clamp and
	// HitStatic flag is set, it means we've collided static from the top.

	if (coll->CollisionType == CollisionType::Clamp &&
		coll->HitStatic && item->Animation.Velocity.y > 0.0f)
	{
		SetAnimation(*item, LA_JUMP_WALL_SMASH_START, 1);
		Rumble(0.5f, 0.15f);

		return true;
	}

	return false;
}

bool LaraDeflectEdgeJump(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (coll->CollisionType != CollisionType::None)
		ShiftItem(item, coll);

	if (coll->CollisionType == CollisionType::Front || coll->CollisionType == CollisionType::TopFront)
	{
		if (!lara->Control.CanClimbLadder || item->Animation.Velocity.z != 2.0f)
		{
			if (coll->Middle.Floor <= CLICK(1))
			{
				if (CanSlide(*item, *coll))
				{
					SetLaraSlideAnimation(item, coll);
				}
				else
				{
					SetAnimation(*item, LA_LAND);
					LaraSnapToHeight(item, coll);
				}
			}
			// TODO: Demagic. This is Lara's running velocity. Jumps have a minimum of 50.
			else if (abs(item->Animation.Velocity.z) > 47.0f)
			{
				SetAnimation(*item, LA_JUMP_WALL_SMASH_START, 1);
				Rumble(0.5f, 0.15f);
			}

			item->Animation.Velocity.z /= 4;
			lara->Control.MoveAngle += ANGLE(180.0f);

			if (item->Animation.Velocity.y <= 0.0f)
				item->Animation.Velocity.y = 1.0f;
		}

		return true;
	}

	switch (coll->CollisionType)
	{
	case CollisionType::Left:
		item->Pose.Orientation.y += DEFLECT_STRAIGHT_ANGLE;
		break;

	case CollisionType::Right:
		item->Pose.Orientation.y -= DEFLECT_STRAIGHT_ANGLE;
		break;

	case CollisionType::Top:
	case CollisionType::TopFront:
		if (item->Animation.Velocity.y <= 0.0f)
			item->Animation.Velocity.y = 1.0f;

		break;

	case CollisionType::Clamp:
		item->Pose.Position.z += CLICK(1.5f) * phd_cos(item->Pose.Orientation.y + ANGLE(180.0f));
		item->Pose.Position.x += CLICK(1.5f) * phd_sin(item->Pose.Orientation.y + ANGLE(180.0f));
		item->Animation.Velocity.z = 0.0f;
		coll->Middle.Floor = 0.0f;

		if (item->Animation.Velocity.y <= 0.0f)
			item->Animation.Velocity.y = 16.0f;

		break;
	}

	return false;
}

void LaraSlideEdgeJump(ItemInfo* item, CollisionInfo* coll)
{
	ShiftItem(item, coll);

	switch (coll->CollisionType)
	{
	case CollisionType::Left:
		item->Pose.Orientation.y += DEFLECT_STRAIGHT_ANGLE;
		break;

	case CollisionType::Right:
		item->Pose.Orientation.y -= DEFLECT_STRAIGHT_ANGLE;
		break;

	case CollisionType::Top:
	case CollisionType::TopFront:
		if (item->Animation.Velocity.y <= 0)
			item->Animation.Velocity.y = 1;

		break;

	case CollisionType::Clamp:
		item->Pose.Position.z += CLICK(1.5f) * phd_cos(item->Pose.Orientation.y + ANGLE(180.0f));
		item->Pose.Position.x += CLICK(1.5f) * phd_sin(item->Pose.Orientation.y + ANGLE(180.0f));
		item->Animation.Velocity.z = 0;
		coll->Middle.Floor = 0;

		if (item->Animation.Velocity.y <= 0)
			item->Animation.Velocity.y = 16;

		break;
	}
}

bool LaraDeflectEdgeCrawl(ItemInfo* item, CollisionInfo* coll)
{
	// Useless in the best case; Lara does not have to embed in order to perform climbing actions in crawl states. Keeping for security. @Sezz 2021.11.26
	if (coll->CollisionType == CollisionType::Front || coll->CollisionType == CollisionType::TopFront)
	{
		ShiftItem(item, coll);

		item->Animation.Velocity.z = 0;
		item->Animation.IsAirborne = false;
		return true;
	}

	if (coll->CollisionType == CollisionType::Left)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y += coll->DiagonalStepAtLeft() ? DEFLECT_DIAGONAL_ANGLE_CRAWL : DEFLECT_STRAIGHT_ANGLE_CRAWL;
	}
	else if (coll->CollisionType == CollisionType::Right)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y -= coll->DiagonalStepAtRight() ? DEFLECT_DIAGONAL_ANGLE_CRAWL : DEFLECT_STRAIGHT_ANGLE_CRAWL;
	}

	return false;
}

bool LaraDeflectEdgeMonkey(ItemInfo* item, CollisionInfo* coll)
{
	// HACK
	if (coll->Shift.Position.y >= 0 && coll->Shift.Position.y <= CLICK(1.25f))
		coll->Shift.Position.y = 0;

	if (coll->CollisionType == CollisionType::Front || coll->CollisionType == CollisionType::TopFront ||
		coll->HitTallObject)
	{
		ShiftItem(item, coll);

		item->Animation.TargetState = LS_MONKEY_IDLE;
		item->Animation.Velocity.z = 0;
		item->Animation.IsAirborne = false;
		return true;
	}

	if (coll->CollisionType == CollisionType::Left)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y += coll->DiagonalStepAtLeft() ? DEFLECT_DIAGONAL_ANGLE : DEFLECT_STRAIGHT_ANGLE;
	}
	else if (coll->CollisionType == CollisionType::Right)
	{
		ShiftItem(item, coll);
		item->Pose.Orientation.y -= coll->DiagonalStepAtRight() ? DEFLECT_DIAGONAL_ANGLE : DEFLECT_STRAIGHT_ANGLE;
	}

	return false;
}

void LaraCollideStop(ItemInfo* item, CollisionInfo* coll)
{
	switch (coll->Setup.PrevState)
	{
	case LS_IDLE:
	case LS_TURN_RIGHT_SLOW:
	case LS_TURN_LEFT_SLOW:
	case LS_TURN_RIGHT_FAST:
	case LS_TURN_LEFT_FAST:
		item->Animation.AnimObjectID = coll->Setup.PrevAnimObjectID;
		item->Animation.AnimNumber = coll->Setup.PrevAnimNumber;
		item->Animation.FrameNumber = coll->Setup.PrevFrameNumber;
		item->Animation.ActiveState = coll->Setup.PrevState;

		if (IsHeld(In::Left))
		{
			// Prevent turn lock against walls.
			if (item->Animation.ActiveState == LS_TURN_RIGHT_SLOW ||
				item->Animation.ActiveState == LS_TURN_RIGHT_FAST)
			{
				item->Animation.TargetState = LS_IDLE;
			}
			else
				item->Animation.TargetState = LS_TURN_LEFT_SLOW;
		}
		else if (IsHeld(In::Right))
		{
			if (item->Animation.ActiveState == LS_TURN_LEFT_SLOW ||
				item->Animation.ActiveState == LS_TURN_LEFT_FAST)
			{
				item->Animation.TargetState = LS_IDLE;
			}
			else
				item->Animation.TargetState = LS_TURN_RIGHT_SLOW;
		}
		else
			item->Animation.TargetState = LS_IDLE;

		AnimateItem(*item);

		break;

	default:
		item->Animation.TargetState = LS_IDLE;

		if (item->Animation.AnimNumber != LA_STAND_SOLID)
			SetAnimation(*item, LA_STAND_SOLID);

		break;
	}
}

void LaraCollideStopCrawl(ItemInfo* item, CollisionInfo* coll)
{
	switch (coll->Setup.PrevState)
	{
	case LS_CRAWL_IDLE:
	case LS_CRAWL_TURN_LEFT:
	case LS_CRAWL_TURN_RIGHT:
		item->Animation.AnimObjectID = coll->Setup.PrevAnimObjectID;
		item->Animation.AnimNumber = coll->Setup.PrevAnimNumber;
		item->Animation.FrameNumber = coll->Setup.PrevFrameNumber;
		item->Animation.ActiveState = coll->Setup.PrevState;

		if (IsHeld(In::Left))
			item->Animation.TargetState = LS_CRAWL_TURN_LEFT;
		else if (IsHeld(In::Right))
			item->Animation.TargetState = LS_CRAWL_TURN_RIGHT;
		else
			item->Animation.TargetState = LS_CRAWL_IDLE;

		AnimateItem(*item);
		break;

	default:
		item->Animation.ActiveState = LS_CRAWL_IDLE;
		item->Animation.TargetState = LS_CRAWL_IDLE;

		if (item->Animation.AnimNumber != LA_CRAWL_IDLE)
		{
			item->Animation.AnimNumber = LA_CRAWL_IDLE;
			item->Animation.FrameNumber = 0;
		}

		break;
	}
}

void LaraCollideStopMonkey(ItemInfo* item, CollisionInfo* coll)
{
	switch (coll->Setup.PrevState)
	{
	case LS_MONKEY_IDLE:
	case LS_MONKEY_TURN_LEFT:
	case LS_MONKEY_TURN_RIGHT:
		item->Animation.AnimObjectID = coll->Setup.PrevAnimObjectID;
		item->Animation.AnimNumber = coll->Setup.PrevAnimNumber;
		item->Animation.FrameNumber = coll->Setup.PrevFrameNumber;
		item->Animation.ActiveState = coll->Setup.PrevState;

		if (IsHeld(In::Left))
			item->Animation.TargetState = LS_MONKEY_TURN_LEFT;
		else if (IsHeld(In::Right))
			item->Animation.TargetState = LS_MONKEY_TURN_RIGHT;
		else
			item->Animation.TargetState = LS_MONKEY_IDLE;

		AnimateItem(*item);
		break;

	default:
		item->Animation.ActiveState = LS_MONKEY_IDLE;
		item->Animation.TargetState = LS_MONKEY_IDLE;

		if (item->Animation.AnimNumber != LA_MONKEY_IDLE)
		{
			item->Animation.AnimNumber = LA_MONKEY_IDLE;
			item->Animation.FrameNumber = 0;
		}

		break;
	}
}

void LaraSnapToEdgeOfBlock(ItemInfo* item, CollisionInfo* coll, short angle)
{
	// Snapping distance of Lara's radius + 12 units is seemingly empirical value from Core tests.
	int snapDistance = coll->Setup.Radius + 12;

	if (item->Animation.ActiveState == LS_SHIMMY_RIGHT)
	{
		switch (angle)
		{
		case NORTH:
			item->Pose.Position.x = (coll->Setup.PrevPosition.x & ~WALL_MASK) | (BLOCK(1) - snapDistance);
			return;

		case EAST:
			item->Pose.Position.z = (coll->Setup.PrevPosition.z & ~WALL_MASK) | snapDistance;
			return;

		case SOUTH:
			item->Pose.Position.x = (coll->Setup.PrevPosition.x & ~WALL_MASK) | snapDistance;
			return;

		case WEST:
		default:
			item->Pose.Position.z = (coll->Setup.PrevPosition.z & ~WALL_MASK) | (BLOCK(1) - snapDistance);
			return;
		}
	}

	if (item->Animation.ActiveState == LS_SHIMMY_LEFT)
	{
		switch (angle)
		{
		case NORTH:
			item->Pose.Position.x = (coll->Setup.PrevPosition.x & ~WALL_MASK) | snapDistance;
			return;

		case EAST:
			item->Pose.Position.z = (coll->Setup.PrevPosition.z & ~WALL_MASK) | (BLOCK(1) - snapDistance);
			return;

		case SOUTH:
			item->Pose.Position.x = (coll->Setup.PrevPosition.x & ~WALL_MASK) | (BLOCK(1) - snapDistance);
			return;

		case WEST:
		default:
			item->Pose.Position.z = (coll->Setup.PrevPosition.z & ~WALL_MASK) | snapDistance;
			return;
		}
	}
}

void LaraResetGravityStatus(ItemInfo* item, CollisionInfo* coll)
{
	// This routine cleans gravity status flag and VerticalVelocity, making it
	// impossible to perform bugs such as QWOP and flare jump. Found by Troye -- Lwmte, 25.09.2021

	if (coll->Middle.Floor <= STEPUP_HEIGHT)
	{
		item->Animation.Velocity.y = 0;
		item->Animation.IsAirborne = false;
	}
}

void LaraSnapToHeight(ItemInfo* item, CollisionInfo* coll)
{
	if (TestEnvironment(ENV_FLAG_SWAMP, item) && coll->Middle.Floor > 0)
		item->Pose.Position.y += g_GameFlow->GetSettings()->Physics.Gravity / SWAMP_GRAVITY_COEFF;
	else if (coll->Middle.Floor != NO_HEIGHT)
		item->Pose.Position.y += coll->Middle.Floor;
}

void GetLaraDeadlyBounds()
{
	auto bounds = GameBoundingBox(LaraItem);
	bounds.Rotate(LaraItem->Pose.Orientation);

	DeadlyBounds = GameBoundingBox(
		LaraItem->Pose.Position.x + bounds.X1,
		LaraItem->Pose.Position.x + bounds.X2,
		LaraItem->Pose.Position.y + bounds.Y1,
		LaraItem->Pose.Position.y + bounds.Y2,
		LaraItem->Pose.Position.z + bounds.Z1,
		LaraItem->Pose.Position.z + bounds.Z2);
}

void LaraJumpCollision(ItemInfo* item, CollisionInfo* coll, short moveAngle)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = moveAngle;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
	coll->Setup.ForwardAngle = lara->Control.MoveAngle;
	GetCollisionInfo(coll, item);

	LaraDeflectEdgeJump(item, coll);
}

void LaraSurfaceCollision(ItemInfo* item, CollisionInfo* coll)
{
	const auto& player = GetLaraInfo(*item);

	coll->Setup.ForwardAngle = player.Control.MoveAngle;

	GetCollisionInfo(coll, item, Vector3i(0, LARA_HEIGHT_TREAD, 0));
	ShiftItem(item, coll);

	if ((coll->CollisionType == CollisionType::Front ||
		coll->CollisionType == CollisionType::Top ||
		coll->CollisionType == CollisionType::TopFront ||
		coll->CollisionType == CollisionType::Clamp) ||
		coll->Middle.Floor < 0 && coll->Middle.FloorSlope)
	{
		item->Animation.Velocity.y = 0;
		item->Pose.Position = coll->Setup.PrevPosition;
	}
	else if (coll->CollisionType == CollisionType::Left)
	{
		item->Pose.Orientation.y += ANGLE(5.0f);
	}
	else if (coll->CollisionType == CollisionType::Right)
	{
		item->Pose.Orientation.y -= ANGLE(5.0f);
	}

	auto pointColl = GetPointCollision(*item);
	if ((pointColl.GetFloorHeight() - item->Pose.Position.y) < SWIM_WATER_DEPTH)
	{
		TestPlayerWaterStepOut(item, coll);
	}
	else if ((pointColl.GetWaterTopHeight() - item->Pose.Position.y) <= -LARA_HEADROOM)
	{
		SetLaraSwimDiveAnimation(item);
	}
}

void LaraDefaultCollision(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeDown = true;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.ForwardAngle = player.Control.MoveAngle;
	GetCollisionInfo(coll, item);
	LaraResetGravityStatus(item, coll);
}

void LaraSwimCollision(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	auto prevPose = item->Pose;

	if (item->Pose.Orientation.x < ANGLE(-90.0f) ||
		item->Pose.Orientation.x > ANGLE(90.0f))
	{
		lara->Control.MoveAngle = item->Pose.Orientation.y + ANGLE(180.0f);
		coll->Setup.ForwardAngle = item->Pose.Orientation.y - ANGLE(180.0f);
	}
	else
	{
		lara->Control.MoveAngle = item->Pose.Orientation.y;
		coll->Setup.ForwardAngle = item->Pose.Orientation.y;
	}

	int height = std::max((int)abs(LARA_HEIGHT * phd_sin(item->Pose.Orientation.x)), LARA_HEIGHT_UNDERWATER);
	auto offset = Vector3i(0, height / 2, 0);

	coll->Setup.UpperFloorBound = -CLICK(0.25f);
	coll->Setup.Height = height;

	GetCollisionInfo(coll, item, offset);

	auto coll0 = *coll;
	coll0.Setup.ForwardAngle += ANGLE(45.0f);
	GetCollisionInfo(&coll0, item, offset);

	auto coll1 = *coll;
	coll1.Setup.ForwardAngle -= ANGLE(45.0f);
	GetCollisionInfo(&coll1, item, offset);

	ShiftItem(item, coll);

	int flag = 0;
	switch (coll->CollisionType)
	{
	case CollisionType::Front:
		if (item->Pose.Orientation.x <= ANGLE(25.0f))
		{
			if (item->Pose.Orientation.x >= -ANGLE(25.0f))
			{
				if (item->Pose.Orientation.x > ANGLE(5.0f))
				{
					item->Pose.Orientation.x += ANGLE(0.5f);
				}
				else if (item->Pose.Orientation.x < -ANGLE(5.0f))
				{
					item->Pose.Orientation.x -= ANGLE(0.5f);
				}
				else if (item->Pose.Orientation.x > 0)
				{
					item->Pose.Orientation.x += 45;
				}
				else if (item->Pose.Orientation.x < 0)
				{
					item->Pose.Orientation.x -= 45;
				}
				else
				{
					item->Animation.Velocity.y = 0;
					flag = 1;
				}
			}
			else
			{
				item->Pose.Orientation.x -= ANGLE(1.0f);
				flag = 1;
			}
		}
		else
		{
			item->Pose.Orientation.x += ANGLE(1.0f);
			flag = 1;
		}

		if (coll0.CollisionType == CollisionType::Left)
		{
			item->Pose.Orientation.y += ANGLE(2.0f);
		}
		else if (coll0.CollisionType == CollisionType::Right)
		{
			item->Pose.Orientation.y -= ANGLE(2.0f);
		}
		else if (coll1.CollisionType == CollisionType::Left)
		{
			item->Pose.Orientation.y += ANGLE(2.0f);
		}
		else if (coll1.CollisionType == CollisionType::Right)
		{
			item->Pose.Orientation.y -= ANGLE(2.0f);
		}

		break;

	case CollisionType::Top:
		if (item->Pose.Orientation.x >= -ANGLE(45.0f))
		{
			item->Pose.Orientation.x -= ANGLE(1.0f);
			flag = 1;
		}

		break;

	case CollisionType::TopFront:
		item->Animation.Velocity.y = 0;
		flag = 1;
		break;

	case CollisionType::Left:
		item->Pose.Orientation.y += ANGLE(2.0f);
		flag = 1;
		break;

	case CollisionType::Right:
		item->Pose.Orientation.y -= ANGLE(2.0f);
		flag = 1;
		break;

	case CollisionType::Clamp:
		item->Animation.Velocity.y = 0.0f;
		item->Pose.Position = coll->Setup.PrevPosition;
		flag = 2;
		break;
	}

	if (coll->Middle.Floor < 0 &&
		coll->Middle.Floor != NO_HEIGHT)
	{
		flag = 1;
		item->Pose.Orientation.x += ANGLE(1.0f);
		item->Pose.Position.y += coll->Middle.Floor;
	}

	if ((prevPose.Position == item->Pose.Position &&
		prevPose.Orientation.x == item->Pose.Orientation.x &&
		prevPose.Orientation.y == item->Pose.Orientation.y) ||
		flag != 1)
	{
		if (flag == 2)
			return;
	}

	if (lara->ExtraAnim == NO_VALUE)
		TestLaraWaterDepth(item, coll);
}

void LaraWaterCurrent(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (lara->Context.WaterCurrentActive)
	{
		const auto& sink = g_Level.Sinks[lara->Context.WaterCurrentActive - 1];

		short headingAngle = Geometry::GetOrientToPoint(item->Pose.Position.ToVector3(), sink.Position).y;
		lara->Context.WaterCurrentPull.x += ((sink.Strength * BLOCK(1) * phd_sin(headingAngle)) - lara->Context.WaterCurrentPull.x) / 16;
		lara->Context.WaterCurrentPull.z += ((sink.Strength * BLOCK(1) * phd_cos(headingAngle)) - lara->Context.WaterCurrentPull.z) / 16;

		item->Pose.Position.y += (sink.Position.y - item->Pose.Position.y) / 16;
	}
	else
	{
		int shift = 0;

		if (abs(lara->Context.WaterCurrentPull.x) <= 16)
			shift = (abs(lara->Context.WaterCurrentPull.x) > 8) + 2;
		else
			shift = 4;
		lara->Context.WaterCurrentPull.x -= lara->Context.WaterCurrentPull.x >> shift;

		if (abs(lara->Context.WaterCurrentPull.x) < 4)
			lara->Context.WaterCurrentPull.x = 0;

		if (abs(lara->Context.WaterCurrentPull.z) <= 16)
			shift = (abs(lara->Context.WaterCurrentPull.z) > 8) + 2;
		else
			shift = 4;
		lara->Context.WaterCurrentPull.z -= lara->Context.WaterCurrentPull.z >> shift;

		if (abs(lara->Context.WaterCurrentPull.z) < 4)
			lara->Context.WaterCurrentPull.z = 0;

		if (!lara->Context.WaterCurrentPull.x && !lara->Context.WaterCurrentPull.z)
			return;
	}

	item->Pose.Position.x += lara->Context.WaterCurrentPull.x / 256;
	item->Pose.Position.z += lara->Context.WaterCurrentPull.z / 256;
	lara->Context.WaterCurrentActive = 0;

	coll->Setup.ForwardAngle = phd_atan(item->Pose.Position.z - coll->Setup.PrevPosition.z, item->Pose.Position.x - coll->Setup.PrevPosition.x);
	coll->Setup.Height = LARA_HEIGHT_CRAWL;
	GetCollisionInfo(coll, item, Vector3i(0, 200, 0));

	if (coll->CollisionType == CollisionType::Front)
	{
		if (item->Pose.Orientation.x > ANGLE(35.0f))
			item->Pose.Orientation.x += ANGLE(1.0f);
		else if (item->Pose.Orientation.x < -ANGLE(35.0f))
			item->Pose.Orientation.x -= ANGLE(1.0f);
		else
			item->Animation.Velocity.y = 0;
	}
	else if (coll->CollisionType == CollisionType::Top)
		item->Pose.Orientation.x -= ANGLE(1.0f);
	else if (coll->CollisionType == CollisionType::TopFront)
		item->Animation.Velocity.y = 0;
	else if (coll->CollisionType == CollisionType::Left)
		item->Pose.Orientation.y += ANGLE(5.0f);
	else if (coll->CollisionType == CollisionType::Right)
		item->Pose.Orientation.y -= ANGLE(5.0f);

	if (coll->Middle.Floor < 0 && coll->Middle.Floor != NO_HEIGHT)
		item->Pose.Position.y += coll->Middle.Floor;

	ShiftItem(item, coll);
	coll->Setup.PrevPosition = item->Pose.Position;
}

bool TestLaraHitCeiling(CollisionInfo* coll)
{
	if (coll->CollisionType == CollisionType::Top ||
		coll->CollisionType == CollisionType::Clamp)
	{
		return true;
	}

	return false;
}

void SetLaraHitCeiling(ItemInfo* item, CollisionInfo* coll)
{
	item->Animation.IsAirborne = false;
	item->Animation.Velocity.y = 0.0f;
	item->Animation.Velocity.z = 0.0f;
	item->Pose.Position = coll->Setup.PrevPosition;
}

bool TestLaraObjectCollision(ItemInfo* item, short headingAngle, int forward, int down, int right)
{
	auto prevPose = item->Pose;
	int sideSign = copysign(1, right);

	// TODO: Use this line?
	//item->Pose.Translate(headingAngle, forward, down, right);

	item->Pose.Position.x += phd_sin(item->Pose.Orientation.y + headingAngle) * forward + phd_cos(headingAngle + ANGLE(90.0f) * sideSign) * abs(right);
	item->Pose.Position.y += down;
	item->Pose.Position.z += phd_cos(item->Pose.Orientation.y + headingAngle) * forward + phd_sin(headingAngle + ANGLE(90.0f) * sideSign) * abs(right);

	bool isCollided = !GetCollidedObjects(*item, true, false).IsEmpty();

	item->Pose = prevPose;
	return isCollided;
}
