#include "framework.h"
#include "Objects/TR3/Entity/Whale.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Specific/level.h"
#include "Game/effects/Ripple.h"
#include "Game/collision/Point.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Ripple;
using namespace TEN::Animation;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto WHALE_FAST_RANGE		= SQUARE(BLOCK(1.5f));
	constexpr auto WHALE_ATTACK1_RANGE	= SQUARE(BLOCK(0.75f));
	constexpr auto WHALE_ATTACK2_RANGE	= SQUARE(BLOCK(1.33f));

	constexpr auto WHALE_SLOW_TURN = ANGLE(2.0f);
	constexpr auto WHALE_FAST_TURN = ANGLE(2.0f);

	enum WhaleState
	{
		WHALE_STATE_SLOW = 0,
		WHALE_STATE_FAST = 1,
		WHALE_STATE_JUMP = 2,
		WHALE_STATE_SPLASH = 3,
		WHALE_STATE_SLOW_BUTT = 4,
		WHALE_STATE_FAST_BUTT = 5,
		WHALE_STATE_BREACH = 6,
		WHALE_STATE_ROLL_180 = 7
	};

	enum WhaleAnim
	{
		WHALE_ANIM_DIE = 4,
		WHALE_ANIM_ROLL_180_FRAME_TURN = 59
	};

	void WhaleControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(&item);

		short angle = 0;

		// Whale is not targetable.
		item.HitPoints = NOT_TARGETABLE;

		AI_INFO ai;
		CreatureAIInfo(&item, &ai);

		// Get mood based on whether Lara is in water or not.
		GetCreatureMood(&item, &ai, true);
		if (!TestEnvironment(ENV_FLAG_WATER, LaraItem->RoomNumber) && Lara.Context.Vehicle == NO_VALUE)
			creature->Mood = MoodType::Bored;

		CreatureMood(&item, &ai, true);
		angle = CreatureTurn(&item, creature->MaxTurn);

		switch (item.Animation.ActiveState)
		{
		case WHALE_STATE_SLOW:
			creature->Flags = 0;
			creature->MaxTurn = WHALE_SLOW_TURN;

			if (creature->Mood == MoodType::Bored)
			{
				if (GetRandomControl() & 0xFF)
					item.Animation.TargetState = WHALE_STATE_SLOW;
				else
					item.Animation.TargetState = WHALE_STATE_JUMP;
			}
			else if (ai.ahead && ai.distance < WHALE_ATTACK1_RANGE)
			{
				item.Animation.TargetState = WHALE_STATE_SLOW_BUTT;
			}
			else if (creature->Mood == MoodType::Escape)
			{
				item.Animation.TargetState = WHALE_STATE_FAST;
			}
			else if (ai.distance > WHALE_FAST_RANGE)
			{
				if (ai.angle < ANGLE(56.25f) && ai.angle > ANGLE(-56.25f))
				{
					if (!(GetRandomControl() & 0x3F))
						item.Animation.TargetState = WHALE_STATE_BREACH;
					else
						item.Animation.TargetState = WHALE_STATE_FAST;
				}
				else
				{
					item.Animation.TargetState = WHALE_STATE_ROLL_180;
				}
			}
			break;

		case WHALE_STATE_FAST:
			creature->Flags = 0;
			creature->MaxTurn = WHALE_FAST_TURN;

			if (creature->Mood == MoodType::Bored)
			{
				if (GetRandomControl() & 0xFF)
					item.Animation.TargetState = WHALE_STATE_SLOW;
				else
					item.Animation.TargetState = WHALE_STATE_JUMP;
			}
			else if (creature->Mood == MoodType::Escape)
			{
				break;
			}
			else if (ai.ahead && ai.distance < WHALE_FAST_RANGE && ai.zoneNumber == ai.enemyZone)
			{
				item.Animation.TargetState = WHALE_STATE_SLOW;
			}
			else if (ai.distance > WHALE_FAST_RANGE && !(GetRandomControl() & 0x7F))
			{
				item.Animation.TargetState = WHALE_STATE_JUMP;
			}
			else if (ai.distance > WHALE_FAST_RANGE && !ai.ahead)
			{
				item.Animation.TargetState = WHALE_STATE_SLOW;
			}
			break;

		case WHALE_STATE_ROLL_180:
			creature->MaxTurn = 0;

			// Turn 180 degrees at specific animation frame.
			if (item.Animation.FrameNumber == WHALE_ANIM_ROLL_180_FRAME_TURN)
			{
				if (item.Pose.Orientation.y < 0)
					item.Pose.Orientation.y += ANGLE(180.0f);
				else
					item.Pose.Orientation.y -= ANGLE(180.0f);

				item.Pose.Orientation.x = -item.Pose.Orientation.x;
			}
			break;
		}

		CreatureAnimation(itemNumber, angle, 0);
		CreatureUnderwater(&item, BLOCK(0.2f));

		// Generate ripples when near water surface.
		if (GlobalCounter & 4)
		{
			auto pos = GetJointPosition(item, 5, Vector3i(-32, 16, -300));
			short roomNumber = item.RoomNumber;
			GetFloor(pos.x, pos.y, pos.z, &roomNumber);

			int waterHeight = GetPointCollision(pos, roomNumber).GetWaterTopHeight();
			if (waterHeight != NO_HEIGHT && pos.y < waterHeight)
			{
				SpawnRipple(Vector3(pos.x, waterHeight, pos.z), item.RoomNumber, Random::GenerateInt(64, 96));
			}
		}
	}
}
