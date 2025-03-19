#include "framework.h"
#include "Objects/TR2/Entity/tr2_yeti.h"

#include "Game/camera.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR2
{
	const auto YetiBiteLeft	 = CreatureBiteInfo(Vector3(12, 101, 19), 13);
	const auto YetiBiteRight = CreatureBiteInfo(Vector3(12, 101, 19), 10);
	const auto YetiLeftPunchAttackJoints  = std::vector<unsigned int>{ 8, 9, 10 };
	const auto YetiRightPunchAttackJoints = std::vector<unsigned int>{ 10, 12 };

	enum YetiState
	{
		YETI_STATE_RUN = 1,
		YETI_STATE_IDLE = 2,
		YETI_STATE_WALK = 3,
		YETI_STATE_ATTACK_IDLE_PUNCH_ATTACK = 4,
		YETI_STATE_ATTACK_IDLE_SLAM_ATTACK = 5,
		YETI_STATE_ATTACK_RUN_PUNCH_ATTACK = 6,
		YETI_STATE_ROAR_START = 7,
		YETI_STATE_DEATH = 8,
		YETI_STATE_ROAR_END = 9,
		YETI_STATE_CLIMB_UP_2_STEPS = 10,
		YETI_STATE_CLIMB_UP_3_STEPS = 11,
		YETI_STATE_CLIMB_UP_4_STEPS = 12,
		YETI_STATE_CLIMB_DOWN_4_STEPS = 13,
		YETI_STATE_KILL = 14
	};

	// TODO
	enum YetiAnim
	{
		YETI_ANIM_DEATH = 31,
		YETI_ANIM_KILL = 36
	};

	void InitializeYeti(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 19);
	}

	void YetiControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		bool isPlayerAlive = LaraItem->HitPoints > 0;

		short headingAngle = 0;
		short tiltAngle = 0;
		short headYRot = 0;
		short torsoYRot = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != YETI_STATE_DEATH)
				SetAnimation(*item, YETI_ANIM_DEATH);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case YETI_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (ai.ahead)
					headYRot = ai.angle;

				if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = 1;
				else if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 128.0f) || !isPlayerAlive)
						item->Animation.TargetState = 7;
					else if (Random::TestProbability(1 / 64.0f))
						item->Animation.TargetState = 9;
					else if (Random::TestProbability(0.025f))
						item->Animation.TargetState = 3;
				}
				else if (ai.ahead && ai.distance < pow(BLOCK(0.5f), 2) && Random::TestProbability(1 / 2.0f))
					item->Animation.TargetState = 4;
				else if (ai.ahead && ai.distance < pow(CLICK(1), 2))
					item->Animation.TargetState = 5;
				else if (creature->Mood == MoodType::Stalk)
					item->Animation.TargetState = 3;
				else
					item->Animation.TargetState = 1;

				break;

			case YETI_STATE_ROAR_START:
				if (ai.ahead)
					headYRot = ai.angle;

				if (creature->Mood == MoodType::Escape || item->HitStatus)
					item->Animation.TargetState = 2;
				else if (creature->Mood == MoodType::Bored)
				{
					if (isPlayerAlive)
					{
						if (Random::TestProbability(1 / 128.0f))
							item->Animation.TargetState = 2;
						else if (Random::TestProbability(1 / 64.0f))
							item->Animation.TargetState = 9;
						else if (Random::TestProbability(0.025f))
						{
							item->Animation.TargetState = 2;
							item->Animation.RequiredState = 3;
						}
					}
				}
				else if (Random::TestProbability(1 / 64.0f))
				{
					item->Animation.TargetState = 2;
				}

				break;

			case YETI_STATE_ROAR_END:
				if (ai.ahead)
					headYRot = ai.angle;

				if (creature->Mood == MoodType::Escape || item->HitStatus)
				{
					item->Animation.TargetState = 2;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 128.0f) || !isPlayerAlive)
					{
						item->Animation.TargetState = 7;
					}
					else if (Random::TestProbability(1 / 64.0f))
					{
						item->Animation.TargetState = 2;
					}
					else if (Random::TestProbability(0.025f))
					{
						item->Animation.TargetState = 2;
						item->Animation.RequiredState = 3;
					}
				}
				else if (Random::TestProbability(1 / 64.0f))
				{
					item->Animation.TargetState = 2;
				}

				break;

			case YETI_STATE_WALK:
				creature->MaxTurn = ANGLE(4.0f);

				if (ai.ahead)
					headYRot = ai.angle;

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = 1;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 128.0f) || !isPlayerAlive)
					{
						item->Animation.TargetState = 2;
						item->Animation.RequiredState = 7;
					}
					else if (Random::TestProbability(1 / 64.0f))
					{
						item->Animation.TargetState = 2;
						item->Animation.RequiredState = 9;
					}
					else if (Random::TestProbability(0.025f))
					{
						item->Animation.TargetState = 2;
					}
				}
				else if (creature->Mood == MoodType::Attack)
				{
					if (ai.ahead && ai.distance < pow(CLICK(1), 2))
					{
						item->Animation.TargetState = 2;
					}
					else if (ai.distance < pow(BLOCK(2), 2))
					{
						item->Animation.TargetState = 1;
					}
				}

				break;

			case YETI_STATE_RUN:
				tiltAngle = headingAngle / 4;
				creature->MaxTurn = ANGLE(6.0f);
				creature->Flags = 0;

				if (ai.ahead)
					headYRot = ai.angle;

				if (creature->Mood == MoodType::Escape)
				{
					break;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					item->Animation.TargetState = 3;
				}
				else if (ai.ahead && ai.distance < pow(CLICK(1), 2))
				{
					item->Animation.TargetState = 2;
				}
				else if (ai.ahead && ai.distance < pow(BLOCK(2), 2))
				{
					item->Animation.TargetState = 6;
				}
				else if (creature->Mood == MoodType::Stalk)
				{
					item->Animation.TargetState = 3;
				}

				break;

			case YETI_STATE_ATTACK_IDLE_PUNCH_ATTACK:
				if (ai.ahead)
					torsoYRot = ai.angle;

				if (!creature->Flags && item->TouchBits.Test(YetiRightPunchAttackJoints))
				{
					CreatureEffect(item, YetiBiteRight, DoBloodSplat);
					DoDamage(creature->Enemy, 100);
					creature->Flags = 1;

					if (LaraItem->HitPoints <= 0)
						CreatureKill(item, YETI_ANIM_KILL, LEA_YETI_DEATH, YETI_ANIM_DEATH, LS_DEATH);
				}

				break;

			case YETI_STATE_ATTACK_IDLE_SLAM_ATTACK:
				creature->MaxTurn = ANGLE(4.0f);

				if (ai.ahead)
					torsoYRot = ai.angle;

				if (!creature->Flags &&
					(item->TouchBits.Test(YetiRightPunchAttackJoints) || item->TouchBits.Test(YetiLeftPunchAttackJoints)))
				{
					if (item->TouchBits.Test(YetiLeftPunchAttackJoints))
						CreatureEffect(item, YetiBiteLeft, DoBloodSplat);

					if (item->TouchBits.Test(YetiRightPunchAttackJoints))
						CreatureEffect(item, YetiBiteRight, DoBloodSplat);

					DoDamage(creature->Enemy, 150);
					creature->Flags = 1;

					if (LaraItem->HitPoints <= 0)
						CreatureKill(item, YETI_ANIM_KILL, LEA_YETI_DEATH, YETI_ANIM_DEATH, LS_DEATH);
				}

				break;

			case YETI_STATE_ATTACK_RUN_PUNCH_ATTACK:
				if (ai.ahead)
					torsoYRot = ai.angle;

				if (!creature->Flags &&
					(item->TouchBits.Test(YetiRightPunchAttackJoints) || item->TouchBits.Test(YetiLeftPunchAttackJoints)))
				{
					if (item->TouchBits.Test(YetiLeftPunchAttackJoints))
						CreatureEffect(item, YetiBiteLeft, DoBloodSplat);

					if (item->TouchBits.Test(YetiRightPunchAttackJoints))
						CreatureEffect(item, YetiBiteRight, DoBloodSplat);

					DoDamage(creature->Enemy, 200);
					creature->Flags = 1;

					if (LaraItem->HitPoints <= 0)
						CreatureKill(item, YETI_ANIM_KILL, LEA_YETI_DEATH, YETI_ANIM_DEATH, LS_DEATH);
				}

				break;

			case YETI_STATE_CLIMB_UP_2_STEPS:
			case YETI_STATE_CLIMB_UP_3_STEPS:
			case YETI_STATE_CLIMB_UP_4_STEPS:
			case YETI_STATE_CLIMB_DOWN_4_STEPS:
				creature->MaxTurn = 0;
				break;

			case YETI_STATE_KILL:
				creature->MaxTurn = 0;

				break;
			}
		}

		CreatureTilt(item, tiltAngle);
		CreatureJoint(item, 0, torsoYRot);
		CreatureJoint(item, 1, headYRot);

		if (item->Animation.ActiveState < 10)
		{
			switch (CreatureVault(itemNumber, headingAngle, 2, 300))
			{
			case 2:
				item->Animation.AnimNumber = 34;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 10;
				break;

			case 3:
				item->Animation.AnimNumber = 33;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 11;
				break;

			case 4:
				item->Animation.AnimNumber = 32;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 12;
				break;

			case -4:
				item->Animation.AnimNumber = 35;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 13;
				break;
			}
		}
		else
		{
			CreatureAnimation(itemNumber, headingAngle, tiltAngle);
		}
	}
}
