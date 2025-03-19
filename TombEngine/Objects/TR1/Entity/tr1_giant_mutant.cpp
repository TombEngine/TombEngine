#include "framework.h"
#include "Objects/TR1/Entity/tr1_giant_mutant.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR1
{
	constexpr auto MUTANT_ATTACK_DAMAGE	 = 500;
	constexpr auto MUTANT_CONTACT_DAMAGE = 6;

	constexpr auto MUTANT_ATTACK_RANGE = SQUARE(BLOCK(5 / 2.0f));
	constexpr auto MUTANT_CLOSE_RANGE  = SQUARE(BLOCK(2.2f));

	// TODO: Chance values are unused. -- Sezz 2022.11.05
	constexpr auto MUTANT_ATTACK_1_CHANCE = 1 / 3.0f;
	constexpr auto MUTANT_ATTACK_2_CHANCE = MUTANT_ATTACK_1_CHANCE * 2;

	constexpr auto MUTANT_NEED_TURN = ANGLE(45.0f);
	constexpr auto MUTANT_TURN	    = ANGLE(3.0f);

	const auto MutantAttackJoints	   = std::vector<unsigned int>{ 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 };
	const auto MutantAttackLeftJoint   = std::vector<unsigned int>{ 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
	const auto MutantAttackRightJoints = std::vector<unsigned int>{ 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 };

	enum GiantMutantState
	{
		MUTANT_STATE_DEATH = 0,
		MUTANT_STATE_IDLE = 1,
		MUTANT_STATE_TURN_LEFT = 2,
		MUTANT_STATE_TURN_RIGHT = 3,
		MUTANT_STATE_SLAM_ATTACK_SINGLE = 4,
		MUTANT_STATE_SLAM_ATTACK_DUAL = 5,
		MUTANT_STATE_SWEEP_ATTACK = 6,
		MUTANT_STATE_FORWARD = 7,
		MUTANT_STATE_SET = 8,
		MUTANT_STATE_FALL = 9,
		// No state 10.
		MUTANT_STATE_KILL = 11
	};

	enum GiantMutantAnim
	{
		MUTANT_ANIM_INACTIVE = 0,
		MUTANT_ANIM_HATCH_END = 1,
		MUTANT_ANIM_IDLE = 2,
		MUTANT_ANIM_IDLE_TO_MOVE_FORWARD = 3,
		MUTANT_ANIM_MOVE_FORWARD_START = 4,
		MUTANT_ANIM_MOVE_FORWARD_END = 5,
		MUTANT_ANIM_MOVE_FORWARD_TO_IDLE = 6,
		MUTANT_ANIM_IDLE_TO_TURN_LEFT = 7,
		MUTANT_ANIM_TURN_LEFT = 8,
		MUTANT_ANIM_TURN_LEFT_TO_IDLE = 9,
		MUTANT_ANIM_SLAM_ATTACK_SINGLE = 10,
		MUTANT_ANIM_SLAM_ATTACK_DOUBLE = 11,
		MUTANT_ANIM_SWEEP_ATTACK = 12,
		MUTANT_ANIM_DEATH = 13,
		MUTANT_ANIM_HATCH_CONT = 14,
		MUTANT_ANIM_HATCH_START = 15,
		MUTANT_ANIM_IDLE_TO_TURN_RIGHT = 16,
		MUTANT_ANIM_TURN_RIGHT = 17,
		MUTANT_ANIM_TURN_RIGHT_TO_IDLE = 18,
		MUTANT_ANIM_KILL = 19
	};

	void GiantMutantControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short headYOrient = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != MUTANT_STATE_DEATH)
				SetAnimation(*item, MUTANT_ANIM_DEATH);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			if (ai.ahead)
				headYOrient = ai.angle;

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = (short)phd_atan(creature->Target.z - item->Pose.Position.z, creature->Target.x - item->Pose.Position.x) - item->Pose.Orientation.y;

			if (item->TouchBits.TestAny())
				DoDamage(creature->Enemy, MUTANT_CONTACT_DAMAGE);

			switch (item->Animation.ActiveState)
			{
			case MUTANT_STATE_SET:
				item->Animation.TargetState = MUTANT_STATE_FALL;
				item->Animation.IsAirborne = true;
				break;

			case MUTANT_STATE_IDLE:
				if (LaraItem->HitPoints <= 0)
					break;

				creature->Flags = 0;

				if (headingAngle > MUTANT_NEED_TURN)
				{
					item->Animation.TargetState = MUTANT_STATE_TURN_RIGHT;
				}
				else if (headingAngle < -MUTANT_NEED_TURN)
				{
					item->Animation.TargetState = MUTANT_STATE_TURN_LEFT;
				}
				else if (ai.distance < MUTANT_ATTACK_RANGE)
				{
					if (LaraItem->HitPoints <= MUTANT_ATTACK_DAMAGE)
					{
						if (ai.distance < MUTANT_CLOSE_RANGE)
						{
							item->Animation.TargetState = MUTANT_STATE_SWEEP_ATTACK;
						}
						else
						{
							item->Animation.TargetState = MUTANT_STATE_FORWARD;
						}
					}
					else if (Random::TestProbability(1 / 2.0f))
					{
						item->Animation.TargetState = MUTANT_STATE_SLAM_ATTACK_SINGLE;
					}
					else
					{
						item->Animation.TargetState = MUTANT_STATE_SLAM_ATTACK_DUAL;
					}
				}
				else
				{
					item->Animation.TargetState = MUTANT_STATE_FORWARD;
				}

				break;

			case MUTANT_STATE_FORWARD:
				if (headingAngle < -MUTANT_TURN)
				{
					item->Animation.TargetState -= MUTANT_TURN;
				}
				else if (headingAngle > MUTANT_TURN)
				{
					item->Animation.TargetState += MUTANT_TURN;
				}
				else
				{
					item->Animation.TargetState += headingAngle;
				}

				if (headingAngle > MUTANT_NEED_TURN || headingAngle < -MUTANT_NEED_TURN)
				{
					item->Animation.TargetState = MUTANT_STATE_IDLE;
				}
				else if (ai.distance < MUTANT_ATTACK_RANGE)
				{
					item->Animation.TargetState = MUTANT_STATE_IDLE;
				}

				break;

			case MUTANT_STATE_TURN_RIGHT:
				if (!creature->Flags)
				{
					creature->Flags = item->Animation.FrameNumber;
				}
				else if (item->Animation.FrameNumber - creature->Flags > 16 &&
					item->Animation.FrameNumber - creature->Flags < 23)
				{
					item->Pose.Orientation.y += ANGLE(14.0f);
				}

				if (headingAngle < MUTANT_NEED_TURN)
					item->Animation.TargetState = MUTANT_STATE_IDLE;

				break;

			case MUTANT_STATE_TURN_LEFT:
				if (!creature->Flags)
				{
					creature->Flags = item->Animation.FrameNumber;
				}
				else if (item->Animation.FrameNumber - creature->Flags > 13 &&
					item->Animation.FrameNumber - creature->Flags < 23)
				{
					item->Pose.Orientation.y -= ANGLE(9.0f);
				}

				if (headingAngle > -MUTANT_NEED_TURN)
					item->Animation.TargetState = MUTANT_STATE_IDLE;

				break;

			case MUTANT_STATE_SLAM_ATTACK_SINGLE:
				if (!creature->Flags && item->TouchBits.Test(MutantAttackRightJoints))
				{
					DoDamage(creature->Enemy, MUTANT_ATTACK_DAMAGE);
					creature->Flags = 1;
				}
				break;

			case MUTANT_STATE_SLAM_ATTACK_DUAL:
				if (!creature->Flags && item->TouchBits.Test(MutantAttackJoints))
				{
					DoDamage(creature->Enemy, MUTANT_ATTACK_DAMAGE);
					creature->Flags = 1;
				}

				break;

			case MUTANT_STATE_SWEEP_ATTACK:
				if (item->TouchBits.Test(MutantAttackRightJoints) || LaraItem->HitPoints <= 0)
				{
					CreatureKill(item, MUTANT_ANIM_KILL, LEA_GIANT_MUTANT_DEATH, MUTANT_STATE_KILL, LS_DEATH);
					Camera.targetDistance = BLOCK(3);
				}

				break;

			case MUTANT_STATE_KILL:
				creature->MaxTurn = 0;
				break;
			}
		}

		CreatureJoint(item, 0, headYOrient);

		if (item->Animation.ActiveState == MUTANT_STATE_FALL)
		{
			AnimateItem(*item);

			if (item->Pose.Position.y > item->Floor)
			{
				item->Animation.TargetState = MUTANT_STATE_IDLE;
				item->Animation.IsAirborne = false;
				item->Pose.Position.y = item->Floor;
				Camera.bounce = 500;
			}
		}
		else
		{
			CreatureAnimation(itemNumber, 0, 0);
		}

		if (item->Status == ITEM_DEACTIVATED)
		{
			SoundEffect(SFX_TR1_ATLANTEAN_DEATH, &item->Pose);
			ExplodingDeath(itemNumber, BODY_DO_EXPLOSION);
		
			TestTriggers(item, true);

			KillItem(itemNumber);
			item->Status = ITEM_DEACTIVATED;
		}
	}
}
