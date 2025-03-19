#include "framework.h"
#include "Objects/TR5/Object/tr5_rollingball.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/Splash.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Objects/Utils/VehicleHelpers.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Splash;

constexpr auto ROLLING_BALL_MAX_VELOCITY = BLOCK(3);

void RollingBallCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	auto* ballItem = &g_Level.Items[itemNumber];

	if (!TestBoundsCollide(ballItem, laraItem, coll->Setup.Radius) ||
		!HandleItemSphereCollision(*ballItem, *laraItem))
	{
		return;
	}

	if (TriggerActive(ballItem) && 
		(ballItem->ItemFlags[0] || ballItem->ItemFlags[1] || ballItem->Animation.Velocity.y))
	{
		laraItem->HitPoints = 0;

		if (!laraItem->Animation.IsAirborne && 
			!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, laraItem))
		{
			SetAnimation(*laraItem, LA_BOULDER_DEATH);

			Camera.flags = CF_FOLLOW_CENTER;
			Camera.targetAngle = ANGLE(170.0f);
			Camera.targetElevation = ANGLE(-25.0f);
			Camera.targetDistance = BLOCK(2);
		}
	}
	else
	{
		ObjectCollision(itemNumber, laraItem, coll);
	}
}

void RollingBallControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (!TriggerActive(item))
		return;

	auto oldPos = item->Pose;

	bool isWater = TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, item->RoomNumber);
	int hDivider = isWater ? 64 : 32;
	int vDivider = isWater ? 3 : 1;

	int smallRadius = CLICK(0.5f);
	int bigRadius   = CLICK(2) - 1;

	item->Animation.Velocity.y += g_GameFlow->GetSettings()->Physics.Gravity;
	item->Pose.Position.x += item->ItemFlags[0] / hDivider;
	item->Pose.Position.y += item->Animation.Velocity.y / vDivider;
	item->Pose.Position.z += item->ItemFlags[1] / hDivider;
	item->Animation.Velocity.z = Vector3i::Distance(item->Pose.Position, oldPos.Position);

	int dh = GetPointCollision(*item).GetFloorHeight() - bigRadius;

	if (item->Pose.Position.y > dh)
	{
		if (abs(item->Animation.Velocity.y) > 16.0f)
		{
			float distance = Vector3::Distance(item->Pose.Position.ToVector3(), Camera.pos.ToVector3());
			if (distance < BLOCK(16))
			{
				if ((item->TriggerFlags & 1) != 1) // Flag 1 = silent.
				{
					Camera.bounce = -((BLOCK(16) - distance) * abs(item->Animation.Velocity.y)) / BLOCK(16);
					SoundEffect(SFX_TR4_BOULDER_FALL, &item->Pose);
				}
			}
		}

		if ((item->Pose.Position.y - dh) < bigRadius)
			item->Pose.Position.y = dh;

		if (item->Animation.Velocity.y <= 64)
		{
			if (abs(item->Animation.Velocity.z) <= CLICK(2) || (GetRandomControl() & 0x1F))
				item->Animation.Velocity.y = 0;
			else
				item->Animation.Velocity.y = -(GetRandomControl() % int(round(item->Animation.Velocity.z) / 8.0f));
		}
		else
		{
			item->Animation.Velocity.y = -item->Animation.Velocity.y / 4.0f;
			item->DisableInterpolation = true;
		}
	}

	int frontX = item->Pose.Position.x;
	int frontZ = item->Pose.Position.z + smallRadius;
	int backX  = item->Pose.Position.x;
	int backZ  = item->Pose.Position.z - smallRadius;
	int rightX = item->Pose.Position.x + smallRadius;
	int rightZ = item->Pose.Position.z;
	int leftX  = item->Pose.Position.x - smallRadius;
	int leftZ  = item->Pose.Position.z;

	auto frontFloor = GetPointCollision(Vector3i(frontX, item->Pose.Position.y, frontZ), item->RoomNumber);
	auto backFloor  = GetPointCollision(Vector3i(backX,  item->Pose.Position.y, backZ),  item->RoomNumber);
	auto rightFloor = GetPointCollision(Vector3i(rightX, item->Pose.Position.y, rightZ), item->RoomNumber);
	auto leftFloor  = GetPointCollision(Vector3i(leftX,  item->Pose.Position.y, leftZ),  item->RoomNumber);

	int frontHeight = frontFloor.GetFloorHeight() - bigRadius;
	int backHeight  = backFloor.GetFloorHeight()  - bigRadius;
	int rightHeight = rightFloor.GetFloorHeight() - bigRadius;
	int leftHeight  = leftFloor.GetFloorHeight()  - bigRadius;

	int frontCeiling = frontFloor.GetCeilingHeight() + bigRadius;
	int backCeiling  = backFloor.GetCeilingHeight()  + bigRadius;
	int rightCeiling = rightFloor.GetCeilingHeight() + bigRadius;
	int leftCeiling  = leftFloor.GetCeilingHeight()  + bigRadius;

	frontX = item->Pose.Position.x;
	frontZ = item->Pose.Position.z + bigRadius;
	backX  = item->Pose.Position.x;
	backZ  = item->Pose.Position.z - bigRadius;
	rightX = item->Pose.Position.x + bigRadius;
	rightZ = item->Pose.Position.z;
	leftX  = item->Pose.Position.x - bigRadius;
	leftZ  = item->Pose.Position.z;

	auto fronFarFloor  = GetPointCollision(Vector3i(frontX, item->Pose.Position.y, frontZ), item->RoomNumber);
	auto backFarFloor  = GetPointCollision(Vector3i(backX,  item->Pose.Position.y, backZ),  item->RoomNumber);
	auto rightFarFloor = GetPointCollision(Vector3i(rightX, item->Pose.Position.y, rightZ), item->RoomNumber);
	auto leftFarFloor  = GetPointCollision(Vector3i(leftX,  item->Pose.Position.y, leftZ),  item->RoomNumber);

	int frontFarHeight = fronFarFloor.GetFloorHeight()  - bigRadius;
	int backFarHeight  = backFarFloor.GetFloorHeight()  - bigRadius;
	int rightFarHeight = rightFarFloor.GetFloorHeight() - bigRadius;
	int leftFarHeight  = leftFarFloor.GetFloorHeight()  - bigRadius;

	int frontFarCeiling = fronFarFloor.GetCeilingHeight()  + bigRadius;
	int backFarCeiling  = backFarFloor.GetCeilingHeight()  + bigRadius;
	int rightFarCeiling = rightFarFloor.GetCeilingHeight() + bigRadius;
	int leftFarCeiling  = leftFarFloor.GetCeilingHeight()  + bigRadius;

	if (item->Pose.Position.y - dh > -CLICK(1) ||
		item->Pose.Position.y - frontFarHeight >= CLICK(2) ||
		item->Pose.Position.y - rightFarHeight >= CLICK(2) ||
		item->Pose.Position.y - backFarHeight  >= CLICK(2) ||
		item->Pose.Position.y - leftFarHeight  >= CLICK(2))
	{
		int counterZ = 0;

		if ((frontFarHeight - dh) <= CLICK(1))
		{
			if (frontFarHeight - dh < -CLICK(4) || frontHeight - dh < -CLICK(1))
			{
				if (item->ItemFlags[1] <= 0)
				{
					if (!item->ItemFlags[1] && item->ItemFlags[0])
						item->Pose.Position.z = (item->Pose.Position.z & ~(CLICK(4) - 1)) | CLICK(2);
				}
				else
				{
					item->ItemFlags[1] = -item->ItemFlags[1] / 2;
					item->Pose.Position.z = (item->Pose.Position.z & ~(CLICK(4) - 1)) | CLICK(2);
				}
			}
			else if (frontHeight == dh)
				counterZ++;
			else
				item->ItemFlags[1] += (frontHeight - dh) / 2;
		}

		if (backHeight - dh <= CLICK(1))
		{
			if (backFarHeight - dh < -CLICK(4) || backHeight - dh < -CLICK(1))
			{
				if (item->ItemFlags[1] >= 0)
				{
					if (!item->ItemFlags[1] && item->ItemFlags[0])
						item->Pose.Position.z = (item->Pose.Position.z & ~(CLICK(4) - 1)) | CLICK(2);
				}
				else
				{
					item->ItemFlags[1] = -item->ItemFlags[1] / 2;
					item->Pose.Position.z = (item->Pose.Position.z & ~(CLICK(4) - 1)) | CLICK(2);
				}
			}
			else if (backHeight == dh)
				counterZ++;
			else
				item->ItemFlags[1] -= (backHeight - dh) / 2;
		}

		if (counterZ == 2)
		{
			if (abs(item->ItemFlags[1]) <= 64)
				item->ItemFlags[1] = 0;
			else
				item->ItemFlags[1] -= (item->ItemFlags[1] / 64);
		}

		int counterX = 0;

		if ((leftHeight - dh) <= CLICK(1))
		{
			if ((leftFarHeight - dh) < -CLICK(4) || leftHeight - dh < -CLICK(1))
			{
				if (item->ItemFlags[0] >= 0)
				{
					if (!item->ItemFlags[0] && item->ItemFlags[1])
						item->Pose.Position.x = (item->Pose.Position.x & ~(CLICK(4) - 1)) | CLICK(2);
				}
				else
				{
					item->ItemFlags[0] = -item->ItemFlags[0] / 2;
					item->Pose.Position.x = (item->Pose.Position.x & ~(CLICK(4) - 1)) | CLICK(2);
				}
			}
			else if (leftHeight == dh)
				counterX++;
			else
				item->ItemFlags[0] -= (leftHeight - dh) / 2;
		}

		if ((rightHeight - dh) <= CLICK(1))
		{
			if ((rightFarHeight - dh) < -CLICK(4) || rightHeight - dh < -CLICK(1))
			{
				if (item->ItemFlags[0] <= 0)
				{
					if (!item->ItemFlags[0] && item->ItemFlags[1])
						item->Pose.Position.x = (item->Pose.Position.x & ~(CLICK(4) - 1)) | CLICK(2);
				}
				else
				{
					item->ItemFlags[0] = -item->ItemFlags[0] / 2;
					item->Pose.Position.x = (item->Pose.Position.x & ~(CLICK(4) - 1)) | CLICK(2);
				}
			}
			else if (rightHeight == dh)
				counterX++;
			else
				item->ItemFlags[0] += (rightHeight - dh) / 2;
		}

		if (counterX == 2)
		{
			if (abs(item->ItemFlags[0]) <= 64)
				item->ItemFlags[0] = 0;
			else
				item->ItemFlags[0] -= (item->ItemFlags[0] / 64);
		}
	}

	auto pointColl = GetPointCollision(*item);
	if (item->RoomNumber != pointColl.GetRoomNumber())
	{
		if (TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, pointColl.GetRoomNumber()) &&
			!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, item->RoomNumber))
		{
			int waterHeight = pointColl.GetWaterTopHeight();
			SplashSetup.Position = Vector3(item->Pose.Position.x, waterHeight - 1, item->Pose.Position.z);
			SplashSetup.SplashPower = item->Animation.Velocity.y * 4;
			SplashSetup.InnerRadius = 160;
			SetupSplash(&SplashSetup, pointColl.GetRoomNumber());
		}

		ItemNewRoom(itemNumber, pointColl.GetRoomNumber());
	}

	if (item->ItemFlags[0] > ROLLING_BALL_MAX_VELOCITY)
		item->ItemFlags[0] = ROLLING_BALL_MAX_VELOCITY;
	else if (item->ItemFlags[0] < -ROLLING_BALL_MAX_VELOCITY)
		item->ItemFlags[0] = -ROLLING_BALL_MAX_VELOCITY;

	if (item->ItemFlags[1] > ROLLING_BALL_MAX_VELOCITY)
		item->ItemFlags[1] = ROLLING_BALL_MAX_VELOCITY;
	else if (item->ItemFlags[1] < -ROLLING_BALL_MAX_VELOCITY)
		item->ItemFlags[1] = -ROLLING_BALL_MAX_VELOCITY;

	short angle = 0;

	if (item->ItemFlags[1] || item->ItemFlags[0])
		angle = phd_atan(item->ItemFlags[1], item->ItemFlags[0]);
	else
		angle = item->Pose.Orientation.y;

	if (item->Pose.Orientation.y != angle)
	{
		if (((angle - item->Pose.Orientation.y) & 0x7fff) >= 512)
		{
			if (angle <= item->Pose.Orientation.y || angle - item->Pose.Orientation.y >= 0x8000)
				item->Pose.Orientation.y -= CLICK(2);
			else
				item->Pose.Orientation.y += CLICK(2);
		}
		else
			item->Pose.Orientation.y = angle;

		item->DisableInterpolation = true;
	}

	item->Pose.Orientation.x -= ((abs(item->ItemFlags[0]) + abs(item->ItemFlags[1])) / 2) / vDivider;

	TestTriggers(item, true);
	DoVehicleCollision(item, bigRadius * 0.9f);
}

void ClassicRollingBallCollision(short itemNum, ItemInfo* lara, CollisionInfo* coll)
{
	auto* item = &g_Level.Items[itemNum];

	if (item->Status == ITEM_ACTIVE)
	{
		if (!TestBoundsCollide(item, lara, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(*item, *lara))
			return;

		if (lara->Animation.IsAirborne)
		{
			if (coll->Setup.EnableObjectPush)
				ItemPushItem(item, lara, coll, coll->Setup.EnableSpasm, 1);

			DoDamage(lara, 100);
			int x = lara->Pose.Position.x - item->Pose.Position.x;
			int y = (lara->Pose.Position.y - 350) - (item->Pose.Position.y - 512);
			int z = lara->Pose.Position.z - item->Pose.Position.z;
			short d = (short)sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));

			if (d < 512)
				d = 512;

			x = item->Pose.Position.x + ((x * 512) / d);
			y = item->Pose.Position.y - 512 + ((y * 512) / d);
			z = item->Pose.Position.z + ((z * 512) / d);
			DoBloodSplat(x, y, z, item->Animation.Velocity.z, item->Pose.Orientation.y, item->RoomNumber);
		}
		else
		{
			lara->HitStatus = true;

			if (lara->HitPoints > 0)
			{
				lara->HitPoints = 0;
				lara->Pose.Orientation.y = item->Pose.Orientation.y;
				lara->Pose.Orientation.x = lara->Pose.Orientation.z = 0;

				SetAnimation(*lara, LA_BOULDER_DEATH);
						
				Camera.flags = CF_FOLLOW_CENTER;
				Camera.targetAngle = ANGLE(170.0f);
				Camera.targetElevation = -ANGLE(-25.0f);
				Camera.targetDistance = BLOCK(2);

				for (int i = 0; i < 15; i++)
				{
					int x = lara->Pose.Position.x + (GetRandomControl() - ANGLE(180.0f) / 256);
					int y = lara->Pose.Position.y - (GetRandomControl() / 64);
					int z = lara->Pose.Position.z + (GetRandomControl() - ANGLE(180.0f) / 256);
					short d = ((GetRandomControl() - ANGLE(180) / 8) + item->Pose.Orientation.y);
					DoBloodSplat(x, y, z, (short)(item->Animation.Velocity.z * 2), d, item->RoomNumber);
				}
			}
		}
	}
	else if (item->Status != ITEM_INVISIBLE)
		ObjectCollision(itemNum, lara, coll);

}

void ClassicRollingBallControl(short itemNum)
{
	int ydist, dist;
	GameVector* old;
	ROOM_INFO* r;

	auto* item = &g_Level.Items[itemNum];

	if (item->Status == ITEM_ACTIVE)
	{
		if (item->Animation.TargetState == 2)
		{
			AnimateItem(*item);
			return;
		}

		if (item->Pose.Position.y < item->Floor)
		{
			if (!item->Animation.IsAirborne)
			{
				item->Animation.IsAirborne = true;
				item->Animation.Velocity.y = -10;
			}
		}
		else if (item->Animation.ActiveState == 0)
			item->Animation.TargetState = 1;

		int oldx = item->Pose.Position.x;
		int oldz = item->Pose.Position.z;

		AnimateItem(*item);

		auto pointColl = GetPointCollision(*item);

		item->Floor = pointColl.GetFloorHeight();

		if (item->RoomNumber != pointColl.GetRoomNumber())
			ItemNewRoom(itemNum, pointColl.GetRoomNumber());

		if (item->Pose.Position.y >= item->Floor - CLICK(1))
		{
			item->Animation.IsAirborne = false;
			item->Animation.Velocity.y = 0;
			item->Pose.Position.y = item->Floor;
			SoundEffect(SFX_TR4_ROLLING_BALL, &item->Pose);
			dist = sqrt((SQUARE(Camera.mikePos.x - item->Pose.Position.x)) + (SQUARE(Camera.mikePos.z - item->Pose.Position.z)));
			if (dist < BLOCK(10))
				Camera.bounce = -40 * (BLOCK(10) - dist) / BLOCK(10);
		}

		if (item->ObjectNumber == ID_CLASSIC_ROLLING_BALL)
		{
			dist = 320;
			ydist = 832;
		}
		else if (item->ObjectNumber == ID_BIG_ROLLING_BALL)
		{
			dist = 1088;
			ydist = 2112;
		}
		else
		{
			dist = BLOCK(1);
			ydist = BLOCK(1);
		}

		int x = item->Pose.Position.x + dist * phd_sin(item->Pose.Orientation.y);
		int z = item->Pose.Position.z + dist * phd_cos(item->Pose.Orientation.y);

		int y1 = GetPointCollision(Vector3i(x, item->Pose.Position.y, z), item->RoomNumber).GetFloorHeight();
		int y2 = GetPointCollision(Vector3i(x, item->Pose.Position.y - ydist, z), item->RoomNumber).GetCeilingHeight();

		if (y1 < item->Pose.Position.y || y2 > (item->Pose.Position.y - ydist))
		{
			StopSoundEffect(SFX_TR4_ROLLING_BALL);
			item->Status = ITEM_DEACTIVATED;
			item->Pose.Position.y = item->Floor;
			item->Pose.Position.x = oldx;
			item->Pose.Position.z = oldz;
			item->Animation.Velocity.z = 0;
			item->Animation.Velocity.y = 0;
			item->TouchBits = NO_JOINT_BITS;
		}
	}
	else if (item->Status == ITEM_DEACTIVATED)
	{
		if (!TriggerActive(item))
		{
			item->Status = ITEM_NOT_ACTIVE;
			old = (GameVector*)item->Data;
			item->Pose.Position.x = old->x;
			item->Pose.Position.y = old->y;
			item->Pose.Position.z = old->z;

			if (item->RoomNumber != old->RoomNumber)
			{
				RemoveDrawnItem(itemNum);
				r = &g_Level.Rooms[old->RoomNumber];
				item->NextItem = r->itemNumber;
				r->itemNumber = itemNum;
				item->RoomNumber = old->RoomNumber;
			}

			item->Animation.AnimNumber = 0;
			item->Animation.FrameNumber = 0;
			item->Animation.ActiveState =
			item->Animation.TargetState = GetAnimData(*item).StateID;
			item->Animation.RequiredState = NO_VALUE;
			RemoveActiveItem(itemNum);
		}
	}

	TestTriggers(item, true);
	DoVehicleCollision(item, CLICK(1.5f));
}

void InitializeClassicRollingBall(short itemNum)
{
	ItemInfo *item;
	GameVector* old;

	item = &g_Level.Items[itemNum];
	item->Data = GameVector{ };
	old = item->Data;
	old->x = item->Pose.Position.x;
	old->y = item->Pose.Position.y;
	old->z = item->Pose.Position.z;
	old->RoomNumber = item->RoomNumber;
}
