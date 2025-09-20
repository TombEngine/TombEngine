#include "framework.h"
#include "Game/Lara/lara_cheat.h"

#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_swim.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/Input/Input.h"

using namespace TEN::Input;

namespace TEN::Entities::Player
{
	void lara_as_fly_cheat(ItemInfo* item, CollisionInfo* coll)
	{
		float baseVel = g_GameFlow->GetSettings()->Physics.SwimVelocity;

		if (IsHeld(In::Forward))
		{
			item->Pose.Orientation.x -= ANGLE(3.0f);
		}
		else if (IsHeld(In::Back))
		{
			item->Pose.Orientation.x += ANGLE(3.0f);
		}

		if (IsHeld(In::Left))
		{
			ModulateLaraTurnRateY(item, ANGLE(3.4f), 0, ANGLE(6.0f));
		}
		else if (IsHeld(In::Right))
		{
			ModulateLaraTurnRateY(item, ANGLE(3.4f), 0, ANGLE(6.0f));
		}

		if (IsHeld(In::Action))
			SpawnDynamicLight(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, 31, 150, 150, 150);

		if (IsHeld(In::Jump))
		{
			float velCoeff = IsHeld(In::Sprint) ? 2.5f : 1.0f;

			item->Animation.Velocity.y += ((baseVel * LARA_SWIM_VELOCITY_ACCEL_COEFF) * 4) * velCoeff;
			if (item->Animation.Velocity.y > (baseVel * 2) * velCoeff)
				item->Animation.Velocity.y = (baseVel * 2) * velCoeff;
		}
		else
		{
			if (item->Animation.Velocity.y >= (baseVel * LARA_SWIM_VELOCITY_ACCEL_COEFF))
			{
				item->Animation.Velocity.y -= item->Animation.Velocity.y / 8;
			}
			else
			{
				item->Animation.Velocity.y = 0.0f;
			}
		}
	}

	void lara_col_fly_cheat(ItemInfo* item, CollisionInfo* coll)
	{
		auto& player = GetLaraInfo(*item);

		if (item->Pose.Orientation.x < ANGLE(-90.0f) ||
			item->Pose.Orientation.x > ANGLE(90.0f))
		{
			player.Control.MoveAngle = item->Pose.Orientation.y + ANGLE(180.0f);
			coll->Setup.ForwardAngle = item->Pose.Orientation.y - ANGLE(180.0f);
		}
		else
		{
			player.Control.MoveAngle = item->Pose.Orientation.y;
			coll->Setup.ForwardAngle = item->Pose.Orientation.y;
		}

		int height = abs(LARA_HEIGHT * phd_sin(item->Pose.Orientation.x));
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

		if (coll->Middle.Floor < 0 &&
			coll->Middle.Floor != NO_HEIGHT)
		{
			item->Pose.Position.y += coll->Middle.Floor;
		}
	}
}
