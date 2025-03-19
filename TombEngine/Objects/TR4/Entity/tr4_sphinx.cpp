#include "framework.h"
#include "Objects/TR4/Entity/tr4_sphinx.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;

namespace TEN::Entities::TR4
{
	constexpr auto SPHINX_ATTACK_DAMAGE = 200;

	constexpr auto SPHINX_WALK_TURN_ANGLE = ANGLE(3.0f);
	constexpr auto SPHINX_RUN_TURN_ANGLE  = ANGLE(0.33f);

	const auto SphinxBite = CreatureBiteInfo(Vector3::Zero, 6);
	const auto SphinxAttackJoints = std::vector<unsigned int>{ 6 };

	enum SphinxState
	{
		// No state 0.
		SPHINX_STATE_NONE = 0,
		SPHINX_STATE_REST = 1,
		SPHINX_STATE_REST_ALERTED = 2,
		SPHINX_STATE_SLEEP_TO_IDLE = 3,
		SPHINX_STATE_WALK_FORWARD = 4,
		SPHINX_STATE_RUN_FORWARD = 5,
		SPHINX_STATE_WALK_BACK = 6,
		SPHINX_STATE_COLLIDE = 7,
		SPHINX_STATE_SHAKE = 8,
		SPHINX_STATE_IDLE = 9
	};

	enum SphinxAnim
	{
		SPHINX_ANIM_RUN_FORWARD = 0,
		SPHINX_ANIM_REST = 1,
		SPHINX_ANIM_REST_ALERTED = 2,
		SPHINX_ANIM_REST_TO_IDLE = 3,
		SPHINX_ANIM_WALK_FORWARD = 4,
		SPHINX_ANIM_IDLE_TO_WALK_FORWARD = 5,
		SPHINX_ANIM_COLLIDE = 6,
		SPHINX_ANIM_SHAKE = 7,
		SPHINX_ANIM_WALK_FORWARD_TO_RUN_FORWARD = 8,
		SPHINX_ANIM_WALK_BACK = 9,
		SPHINX_ANIM_IDLE = 10,
		SPHINX_ANIM_RUN_FORWARD_TO_IDLE = 11,
		SPHINX_ANIM_WALK_FORWARD_TO_IDLE = 12,
		SPHINX_ANIM_IDLE_TO_RUN_FORWARD = 13,
		SPHINX_ANIM_IDLE_TO_WALK_BACK = 14,
		SPHINX_ANIM_WALK_BACK_TO_IDLE = 15
	};

	void InitializeSphinx(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, SPHINX_ANIM_REST);
	}

	void SphinxControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		int x = item->Pose.Position.x + 614 * phd_sin(item->Pose.Orientation.y);
		int y = item->Pose.Position.y;
		int z = item->Pose.Position.z + 614 * phd_cos(item->Pose.Orientation.y);

		auto pointColl = GetPointCollision(Vector3i(x, y, z), item->RoomNumber);

		int height1 = pointColl.GetFloorHeight();

		if (item->Animation.ActiveState == SPHINX_STATE_RUN_FORWARD && pointColl.GetSector().Stopper)
		{
			auto* room = &g_Level.Rooms[item->RoomNumber];

			for (int i = 0; i < room->mesh.size(); i++)
			{
				auto* mesh = &room->mesh[i];

				if (((mesh->pos.Position.z / BLOCK(1)) == (z / BLOCK(1))) &&
					((mesh->pos.Position.x / BLOCK(1)) == (x / BLOCK(1))) &&
					Statics[mesh->staticNumber].shatterType != ShatterType::None)
				{
					ShatterObject(nullptr, mesh, -64, item->RoomNumber, 0);
					SoundEffect(SFX_TR4_SMASH_ROCK, &item->Pose);

					TestTriggers(x, y, z, item->RoomNumber, true);
				}
			}
		}

		x = item->Pose.Position.x - 614 * phd_sin(item->Pose.Orientation.y);
		y = item->Pose.Position.y;
		z = item->Pose.Position.z - 614 * phd_cos(item->Pose.Orientation.y);

		int height2 = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();

		phd_atan(1228, height2 - height1);

		if (item->AIBits)
		{
			GetAITarget(creature);
		}
		else
		{
			creature->Enemy = LaraItem;
		}

		AI_INFO ai;
		CreatureAIInfo(item, &ai);

		if (!creature->Enemy->IsLara())
			phd_atan(LaraItem->Pose.Position.z - item->Pose.Position.z, LaraItem->Pose.Position.x - item->Pose.Position.x);

		GetCreatureMood(item, &ai, true);
		CreatureMood(item, &ai, true);

		short angle = CreatureTurn(item, creature->MaxTurn);

		int dx = abs(item->ItemFlags[2] - (short)item->Pose.Position.x);
		int dz = abs(item->ItemFlags[3] - (short)item->Pose.Position.z);

		switch (item->Animation.ActiveState)
		{
		case SPHINX_STATE_REST:
			creature->MaxTurn = 0;

			if (ai.distance < pow(BLOCK(1), 2) || item->TriggerFlags)
				item->Animation.TargetState = SPHINX_STATE_SLEEP_TO_IDLE;

			// TODO: Use TestProbability().
			if (GetRandomControl() == 0)
				item->Animation.TargetState = SPHINX_STATE_REST_ALERTED;

			break;

		case SPHINX_STATE_REST_ALERTED:
			creature->MaxTurn = 0;

			if (ai.distance < pow(BLOCK(1), 2) || item->TriggerFlags)
				item->Animation.TargetState = SPHINX_STATE_SLEEP_TO_IDLE;

			// TODO: Use TestProbability().
			if (GetRandomControl() == 0)
				item->Animation.TargetState = SPHINX_STATE_REST;

			break;

		case SPHINX_STATE_WALK_FORWARD:
			creature->MaxTurn = SPHINX_WALK_TURN_ANGLE;

			if (ai.distance > pow(BLOCK(1), 2) && abs(ai.angle) <= ANGLE(2.8f) ||
				item->Animation.RequiredState == SPHINX_STATE_RUN_FORWARD)
			{
				item->Animation.TargetState = SPHINX_STATE_RUN_FORWARD;
			}
			else if (ai.distance < pow(BLOCK(2), 2) && item->Animation.TargetState != SPHINX_STATE_RUN_FORWARD)
			{
				if (height2 <= (item->Pose.Position.y + CLICK(1)) &&
					height2 >= (item->Pose.Position.y - CLICK(1)))
				{
					item->Animation.TargetState = SPHINX_STATE_IDLE;
					item->Animation.RequiredState = SPHINX_STATE_WALK_BACK;
				}
			}

			break;

		case SPHINX_STATE_RUN_FORWARD:
			creature->MaxTurn = SPHINX_RUN_TURN_ANGLE;

			if (creature->Flags == 0)
			{
				if (item->TouchBits.Test(SphinxAttackJoints))
				{
					DoDamage(creature->Enemy, SPHINX_ATTACK_DAMAGE);
					CreatureEffect2(item, SphinxBite, 20, -1, DoBloodSplat);
					creature->Flags = 1;
				}
			}

			if (dx >= 50 || dz >= 50 ||
				item->Animation.AnimNumber != 0)
			{
				if (ai.distance > pow(BLOCK(2), 2) && abs(ai.angle) > ANGLE(2.8f))
					item->Animation.TargetState = SPHINX_STATE_IDLE;
			}
			else
			{
				item->Animation.TargetState = SPHINX_STATE_COLLIDE;
				item->Animation.RequiredState = SPHINX_STATE_WALK_BACK;
				creature->MaxTurn = 0;
			}

			break;

		case SPHINX_STATE_WALK_BACK:
			creature->MaxTurn = SPHINX_WALK_TURN_ANGLE;

			if (ai.distance > pow(BLOCK(2), 2) ||
				height2 > (item->Pose.Position.y + CLICK(1)) ||
				height2 < (item->Pose.Position.y - CLICK(1)))
			{
				item->Animation.TargetState = SPHINX_STATE_IDLE;
				item->Animation.RequiredState = SPHINX_STATE_RUN_FORWARD;
			}

			break;

		case SPHINX_STATE_COLLIDE:
			if (item->Animation.FrameNumber == 0)
			{
				TestTriggers(item, true);

				if (item->TouchBits.Test(SphinxAttackJoints))
				{
					DoDamage(creature->Enemy, INT_MAX);
					CreatureEffect2(item, SphinxBite, 50, -1, DoBloodSplat);
				}
			}

			break;

		case SPHINX_STATE_IDLE:
			creature->Flags = 0;

			if (item->Animation.RequiredState == SPHINX_STATE_WALK_BACK)
			{
				item->Animation.TargetState = SPHINX_STATE_WALK_BACK;
			}
			else
			{
				item->Animation.TargetState = SPHINX_STATE_WALK_FORWARD;
			}

			break;

		default:
			break;
		}

		item->ItemFlags[2] = (short)item->Pose.Position.x;
		item->ItemFlags[3] = (short)item->Pose.Position.z;

		CreatureAnimation(itemNumber, angle, 0);
	}
}
