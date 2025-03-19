#include "framework.h"
#include "Objects/TR2/Entity/tr2_bird_monster.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto BIRD_MONSTER_ATTACK_DAMAGE = 200;
	constexpr auto BIRD_MONSTER_SLAM_CRUSH_ATTACK_RANGE = SQUARE(BLOCK(1));
	constexpr auto BIRD_MONSTER_PUNCH_ATTACK_RANGE		= SQUARE(BLOCK(2));

	constexpr auto BIRD_MONSTER_WALK_TURN_RATE_MAX = ANGLE(4.0f);

	const auto BirdMonsterBiteLeft	= CreatureBiteInfo(Vector3(0, 224, 0), 19);
	const auto BirdMonsterBiteRight = CreatureBiteInfo(Vector3(0, 224, 0), 22);
	const auto BirdMonsterAttackLeftJoints  = std::vector<unsigned int>{ 18, 19 };
	const auto BirdMonsterAttackRightJoints = std::vector<unsigned int>{ 21, 22 };

	enum BirdMonsterState
	{
		// No state 0.
		BMONSTER_STATE_IDLE = 1,
		BMONSTER_STATE_WALK_FORWARD = 2,
		BMONSTER_STATE_SLAM_ATTACK_START = 3,
		BMONSTER_STATE_SLAM_ATTACK_CONTINUE = 4,
		BMONSTER_STATE_PUNCH_ATTACK_START = 5,
		BMONSTER_STATE_PUNCH_ATTACK_RIGHT_CONTINUE = 6,
		BMONSTER_STATE_PUNCH_ATTACK_LEFT_CONTINUE = 7,
		BMONSTER_STATE_ROAR = 8,
		BMONSTER_STATE_DEATH = 9,
		BMONSTER_STATE_CRUSH_ATTACK_START = 10,
		BMONSTER_STATE_CRUSH_ATTACK_CONTINUE = 11
	};

	enum BirdMonsterAnim
	{
		BMONSTER_ANIM_IDLE = 0,
		BMONSTER_ANIM_IDLE_TO_WALK_FORWARD = 1,
		BMONSTER_ANIM_WALK_FORWARD = 2,
		BMONSTER_ANIM_WALK_FORWARD_TO_IDLE_LEFT = 3,
		BMONSTER_ANIM_WALK_FORWARD_TO_IDLE_RIGHT = 4,
		BMONSTER_ANIM_SLAM_ATTACK_START = 5,
		BMONSTER_ANIM_SLAM_ATTACK_CANCEL = 6,
		BMONSTER_ANIM_SLAM_ATTACK_CONTINUE = 7,
		BMONSTER_ANIM_SLAM_ATTACK_END = 8,
		BMONSTER_ANIM_PUNCH_ATTACK_RIGHT_START = 9,
		BMONSTER_ANIM_PUNCH_ATTACK_RIGHT_CANCEL = 10,
		BMONSTER_ANIM_PUNCH_ATTACK_RIGHT_CONTINUE = 11,
		BMONSTER_ANIM_PUNCH_ATTACK_RIGHT_END = 12,
		BMONSTER_ANIM_PUNCH_ATTACK_LEFT_START = 13,
		BMONSTER_ANIM_PUNCH_ATTACK_LEFT_CANCEL = 14,
		BMONSTER_ANIM_PUNCH_ATTACK_LEFT_CONTINUE = 15,
		BMONSTER_ANIM_PUNCH_ATTACK_LEFT_END = 16,
		BMONSTER_ANIM_ROAR_START = 17,
		BMONSTER_ANIM_ROAR_CONTINUE = 18,
		BMONSTER_ANIM_ROAR_END = 19,
		BMONSTER_ANIM_DEATH = 20,
		BMONSTER_ANIM_CRUSH_ATTACK_START = 21,
		BMONSTER_ANIM_CRUSH_ATTACK_CANCEL = 22,
		BMONSTER_ANIM_CRUSH_ATTACK_CONTINUE = 23,
		BMONSTER_ANIM_CRUSH_ATTACK_END = 24
	};

	void BirdMonsterControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short head = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != BMONSTER_STATE_DEATH)
				SetAnimation(*item, BMONSTER_ANIM_DEATH);
		}
		else
		{
			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			if (AI.ahead)
				head = AI.angle;

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);
			angle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case BMONSTER_STATE_IDLE:
				creature->MaxTurn = 0;

				if (AI.ahead && AI.distance < BIRD_MONSTER_SLAM_CRUSH_ATTACK_RANGE)
				{
					if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = BMONSTER_STATE_SLAM_ATTACK_START;
					else
						item->Animation.TargetState = BMONSTER_STATE_CRUSH_ATTACK_START;
				}
				else if (AI.ahead &&
					(creature->Mood == MoodType::Bored || creature->Mood == MoodType::Stalk))
				{
					if (AI.zoneNumber != AI.enemyZone)
					{
						item->Animation.TargetState = BMONSTER_STATE_WALK_FORWARD;
						creature->Mood = MoodType::Escape;
					}
					else
						item->Animation.TargetState = BMONSTER_STATE_ROAR;
				}
				else
					item->Animation.TargetState = BMONSTER_STATE_WALK_FORWARD;

				break;

			case BMONSTER_STATE_ROAR:
				creature->MaxTurn = 0;

				if (creature->Mood != MoodType::Bored || !AI.ahead)
					item->Animation.TargetState = BMONSTER_STATE_IDLE;

				break;

			case BMONSTER_STATE_WALK_FORWARD:
				creature->MaxTurn = BIRD_MONSTER_WALK_TURN_RATE_MAX;

				if (AI.ahead && AI.distance < BIRD_MONSTER_PUNCH_ATTACK_RANGE)
					item->Animation.TargetState = BMONSTER_STATE_PUNCH_ATTACK_START;
				else if ((creature->Mood == MoodType::Bored || creature->Mood == MoodType::Stalk) && AI.ahead)
					item->Animation.TargetState = BMONSTER_STATE_IDLE;

				break;

			case BMONSTER_STATE_SLAM_ATTACK_START:
				creature->Flags = 0;

				if (AI.ahead && AI.distance < BIRD_MONSTER_SLAM_CRUSH_ATTACK_RANGE)
					item->Animation.TargetState = BMONSTER_STATE_SLAM_ATTACK_CONTINUE;
				else
					item->Animation.TargetState = BMONSTER_STATE_IDLE;

				break;

			case BMONSTER_STATE_PUNCH_ATTACK_START:
				creature->Flags = 0;

				if (AI.ahead && AI.distance < BIRD_MONSTER_PUNCH_ATTACK_RANGE)
					item->Animation.TargetState = BMONSTER_STATE_PUNCH_ATTACK_RIGHT_CONTINUE;
				else
					item->Animation.TargetState = BMONSTER_STATE_IDLE;

				break;

			case BMONSTER_STATE_CRUSH_ATTACK_START:
				creature->Flags = 0;

				if (AI.ahead && AI.distance < BIRD_MONSTER_SLAM_CRUSH_ATTACK_RANGE)
					item->Animation.TargetState = BMONSTER_STATE_CRUSH_ATTACK_CONTINUE;
				else
					item->Animation.TargetState = BMONSTER_STATE_IDLE;

				break;

			case BMONSTER_STATE_SLAM_ATTACK_CONTINUE:
			case BMONSTER_STATE_PUNCH_ATTACK_RIGHT_CONTINUE:
			case BMONSTER_STATE_CRUSH_ATTACK_CONTINUE:
			case BMONSTER_STATE_PUNCH_ATTACK_LEFT_CONTINUE:
				if (!(creature->Flags & 1) &&
					item->TouchBits.Test(BirdMonsterAttackRightJoints))
				{
					DoDamage(creature->Enemy, BIRD_MONSTER_ATTACK_DAMAGE);
					CreatureEffect(item, BirdMonsterBiteRight, DoBloodSplat);
					creature->Flags |= 1;
				}

				if (!(creature->Flags & 2) &&
					item->TouchBits.Test(BirdMonsterAttackLeftJoints))
				{
					DoDamage(creature->Enemy, BIRD_MONSTER_ATTACK_DAMAGE);
					CreatureEffect(item, BirdMonsterBiteLeft, DoBloodSplat);
					creature->Flags |= 2;
				}

				break;
			}
		}

		CreatureJoint(item, 0, head);
		CreatureAnimation(itemNumber, angle, 0);
	}
}
