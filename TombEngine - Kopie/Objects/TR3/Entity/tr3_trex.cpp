#include "framework.h"
#include "Objects/TR3/Entity/tr3_trex.h"

#include "Game/camera.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto TREX_CONTACT_DAMAGE	   = 1;
	constexpr auto TREX_RUN_CONTACT_DAMAGE = 10;
	constexpr auto TREX_ROAR_CHANCE	= 1.0f / 64;

	constexpr auto TREX_WALK_TURN_RATE_MAX = ANGLE(2.0f);
	constexpr auto TREX_RUN_TURN_RATE_MAX  = ANGLE(4.0f);

	const auto TRexAttackJoints = std::vector<unsigned int>{ 12, 13 };

	enum TRexState
	{
		// No state 0.
		TREX_STATE_IDLE = 1,
		TREX_STATE_WALK_FORWARD = 2,
		TREX_STATE_RUN_FORWARD = 3,
		// No state 4.
		TREX_STATE_DEATH = 5,
		TREX_STATE_ROAR = 6,
		TREX_STATE_ATTACK = 7,
		TREX_STATE_KILL = 8,
		
		// TODO: TR3 states.
		/*TREX_STATE_ROAR_START = 9,
		TREX_STATE_ROAR_CONT = 10,
		TREX_STATE_ROAR_END = 11,
		TREX_STATE_HUNCH_START = 12,
		TREX_STATE_HUNCH_LOOP = 13,
		TREX_STATE_HUNCH_END = 14*/
	};

	enum TRexAnim
	{
		TREX_ANIM_IDLE = 0,
		TREX_ANIM_RUN_FORWARD = 1,
		TREX_ANIM_WALK_FORWARD = 2,
		TREX_ANIM_IDLE_TO_WALK_FORWARD = 3,
		TREX_ANIM_WALK_FORWARD_TO_IDLE = 4,
		TREX_ANIM_ROAR = 5,
		TREX_ANIM_ATTACK = 6,
		TREX_ANIM_IDLE_TO_RUN_START = 7,
		TREX_ANIM_IDLE_TO_RUN_END = 8,
		TREX_ANIM_RUN_FORWARD_TO_IDLE_RIGHT = 9,
		TREX_ANIM_DEATH = 10,
		TREX_ANIM_KILL = 11,
		
		// TODO: TR3 anims.
		/*TREX_ANIM_ROARING_START = 12,
		TREX_ANIM_ROARING_LOOP = 13,
		TREX_ANIM_ROARING_END = 14,
		TREX_ANIM_HUNCH_START = 15,
		TREX_ANIM_HUNCH_LOOP = 16,
		TREX_ANIM_HUNCH_END = 17,

		// TODO: Missing in TR1 object.
		TREX_ANIM_RUN_FORWARD_TO_IDLE_LEFT = 18*/
	};

	void TRexControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short headYRot = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState == TREX_STATE_IDLE)
			{
				item->Animation.TargetState = TREX_STATE_DEATH;
			}
			else
			{
				item->Animation.TargetState = TREX_STATE_IDLE;
			}
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			if (ai.ahead)
				headYRot = ai.angle;

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			if (item->TouchBits.TestAny())
				DoDamage(LaraItem, (item->Animation.ActiveState == TREX_STATE_RUN_FORWARD) ? TREX_RUN_CONTACT_DAMAGE : TREX_CONTACT_DAMAGE);

			creature->Flags = (creature->Mood != MoodType::Escape && !ai.ahead && ai.enemyFacing > -FRONT_ARC && ai.enemyFacing < FRONT_ARC);

			if (ai.distance > pow(1500, 2) &&
				ai.distance < pow(BLOCK(4), 2) &&
				ai.bite && !creature->Flags)
			{
				creature->Flags = 1;
			}

			switch (item->Animation.ActiveState)
			{
			case TREX_STATE_IDLE:
				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (ai.distance < pow(1500, 2) && ai.bite)
					item->Animation.TargetState = TREX_STATE_ATTACK;
				else if (creature->Mood == MoodType::Bored || creature->Flags)
					item->Animation.TargetState = TREX_STATE_WALK_FORWARD;
				else
					item->Animation.TargetState = TREX_STATE_RUN_FORWARD;

				break;

			case TREX_STATE_WALK_FORWARD:
				creature->MaxTurn = TREX_WALK_TURN_RATE_MAX;

				if (creature->Mood != MoodType::Bored || !creature->Flags)
					item->Animation.TargetState = TREX_STATE_IDLE;
				else if (ai.ahead && Random::TestProbability(TREX_ROAR_CHANCE))
				{
					item->Animation.TargetState = TREX_STATE_IDLE;
					item->Animation.RequiredState = TREX_STATE_ROAR;
				}

				break;

			case TREX_STATE_RUN_FORWARD:
				creature->MaxTurn = TREX_RUN_TURN_RATE_MAX;

				if (ai.distance < pow(BLOCK(5), 2) && ai.bite)
					item->Animation.TargetState = TREX_STATE_IDLE;
				else if (creature->Flags)
					item->Animation.TargetState = TREX_STATE_IDLE;
				else if (creature->Mood != MoodType::Escape &&
					ai.ahead && Random::TestProbability(TREX_ROAR_CHANCE))
				{
					item->Animation.TargetState = TREX_STATE_IDLE;
					item->Animation.RequiredState = TREX_STATE_ROAR;
				}
				else if (creature->Mood == MoodType::Bored)
					item->Animation.TargetState = TREX_STATE_IDLE;

				break;

			case TREX_STATE_ATTACK:
				if (item->TouchBits.Test(TRexAttackJoints))
				{
					CreatureKill(item, TREX_ANIM_KILL, LEA_TREX_DEATH, TREX_STATE_KILL, LS_DEATH);
					Camera.targetDistance = BLOCK(3);
				}

				break;

			case TREX_STATE_KILL:
				headYRot = 0;
				creature->MaxTurn = 0;
				break;
			}
		}

		CreatureJoint(item, 0, headYRot / 2);
		CreatureJoint(item, 1, headYRot / 2);

		CreatureAnimation(itemNumber, headingAngle, 0);

		item->Collidable = true;
	}
}
