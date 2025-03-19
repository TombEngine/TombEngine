#include "framework.h"
#include "Objects/TR2/Entity/tr2_silencer.h"

#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto SILENCER_SHOOT_ATTACK_DAMAGE = 50;
	constexpr auto SILENCER_RUN_RANGE = SQUARE(BLOCK(2));

	constexpr auto SILENCER_WALK_TURN_RATE_MAX = ANGLE(5.0f);
	constexpr auto SILENCER_RUN_TURN_RATE_MAX  = ANGLE(5.0f);

	const auto SilencerGunBite = CreatureBiteInfo(Vector3(-10, 360, 60), 10);

	enum SilencerState
	{
		// No state 0.
		SILENCER_STATE_WALK_FORWARD = 1,
		SILENCER_STATE_RUN_FORWARD = 2,
		SILENCER_STATE_IDLE_FRAME = 3,
		SILENCER_STATE_IDLE = 4,
		SILENCER_STATE_POSE = 5,
		SILENCER_STATE_AIM_1 = 6,
		SILENCER_STATE_SHOOT_1 = 7,
		// No state 8.
		SILENCER_STATE_RUN_SHOOT = 9,
		SILENCER_STATE_AIM_2 = 10,
		SILENCER_STATE_SHOOT_2 = 11,
		SILENCER_STATE_DEATH_1 = 12,
		SILENCER_STATE_DEATH_2 = 13
	};

	enum SilencerAnim
	{
		SILENCER_ANIM_IDLE_FRAME = 0,
		SILENCER_ANIM_IDLE_TO_WALK_FORWARD = 1,
		SILENCER_ANIM_WALK_FORWARD = 2,
		SILENCER_ANIM_WALK_FORWARD_TO_IDLE = 3,
		SILENCER_ANIM_WALK_FORWARD_TO_POSE = 4,
		SILENCER_ANIM_POSE = 5,
		SILENCER_ANIM_WALK_FORWARD_TO_RUN_FORWARD = 6,
		SILENCER_ANIM_RUN_FORWARD = 7,
		SILENCER_ANIM_RUN_FORWARD_TO_IDLE = 8,
		SILENCER_ANIM_IDLE_TO_RUN_FORWARD = 9,
		SILENCER_ANIM_POSE_TO_IDLE = 10,
		SILENCER_ANIM_RUN_FORWARD_AIM_LEFT = 11,
		SILENCER_ANIM_RUN_FORWARD_SHOOT_LEFT = 12,
		SILENCER_ANIM_RUN_FORWARD_UNAIM_LEFT = 13,
		SILENCER_ANIM_AIM_1_START = 14,
		SILENCER_ANIM_AIM_1_CONTINUE = 15,
		SILENCER_ANIM_SHOOT_1 = 16,
		SILENCER_ANIM_UNAIM_1 = 17,
		SILENCER_ANIM_POSE_TO_AIM_1 = 18,
		SILENCER_ANIM_IDLE = 19,
		SILENCER_ANIM_DEATH_1 = 20,
		SILENCER_ANIM_DEATH_2 = 21, // Unused.
		SILENCER_ANIM_AIM_2_START = 22,
		SILENCER_ANIM_AIM_2_CONTINUE = 23,
		SILENCER_ANIM_SHOOT_2 = 24,
		SILENCER_ANIM_UNAIM_2 = 25,
		SILENCER_ANIM_RUN_FORWARD_AIM_RIGHT = 26,
		SILENCER_ANIM_RUN_FORWARD_SHOOT_RIGHT = 27,
		SILENCER_ANIM_RUN_FORWARD_UNAIM_RIGHT = 28
	};

	void SilencerControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != SILENCER_STATE_DEATH_1 &&
				item->Animation.ActiveState != SILENCER_STATE_DEATH_2)
			{
				SetAnimation(*item, SILENCER_ANIM_DEATH_1);
			}
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
			case SILENCER_STATE_IDLE_FRAME:
				creature->MaxTurn = 0;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;

				break;

			case SILENCER_STATE_IDLE:
				creature->MaxTurn = 0;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
					item->Animation.RequiredState = SILENCER_STATE_RUN_FORWARD;
				}
				else
				{
					if (Targetable(item, &ai))
					{
						item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
						item->Animation.RequiredState = Random::TestProbability(1 / 2.0f) ? SILENCER_STATE_AIM_1 : SILENCER_STATE_AIM_2;
					}

					if (creature->Mood == MoodType::Attack || !ai.ahead)
					{
						if (ai.distance >= SILENCER_RUN_RANGE)
						{
							item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
							item->Animation.RequiredState = SILENCER_STATE_RUN_FORWARD;
						}
						else
						{
							item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
							item->Animation.RequiredState = SILENCER_STATE_WALK_FORWARD;
						}
					}
					else
					{
						if (Random::TestProbability(0.96f))
						{
							if (Random::TestProbability(0.08f))
							{
								item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
								item->Animation.RequiredState = SILENCER_STATE_WALK_FORWARD;
							}
						}
						else
						{
							item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
							item->Animation.RequiredState = SILENCER_STATE_POSE;
						}
					}
				}

				break;

			case SILENCER_STATE_WALK_FORWARD:
				creature->MaxTurn = SILENCER_WALK_TURN_RATE_MAX;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = SILENCER_STATE_RUN_FORWARD;
				}
				else if (Targetable(item, &ai))
				{
					item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
					item->Animation.RequiredState = Random::TestProbability(1 / 2.0f) ? SILENCER_STATE_AIM_1 : SILENCER_STATE_AIM_2;
				}
				else
				{
					if (ai.distance > SILENCER_RUN_RANGE || !ai.ahead)
						item->Animation.TargetState = SILENCER_STATE_RUN_FORWARD;
					if (creature->Mood == MoodType::Bored && Random::TestProbability(0.025f))
						item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
				}

				break;

			case SILENCER_STATE_RUN_FORWARD:
				creature->MaxTurn = SILENCER_RUN_TURN_RATE_MAX;
				creature->Flags = 0;
				tiltAngle = headingAngle / 4;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (creature->Mood == MoodType::Escape)
				{
					if (Targetable(item, &ai))
						item->Animation.TargetState = SILENCER_STATE_RUN_SHOOT;

					break;
				}

				if (Targetable(item, &ai))
				{
					if (ai.distance >= SILENCER_RUN_RANGE && ai.zoneNumber == ai.enemyZone)
						item->Animation.TargetState = SILENCER_STATE_RUN_SHOOT;

					break;
				}
				else if (creature->Mood == MoodType::Attack)
				{
					item->Animation.TargetState = Random::TestProbability(1 / 2.0f) ? SILENCER_STATE_RUN_FORWARD : SILENCER_STATE_IDLE_FRAME;
				}
				else
				{
					item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
				}

				break;

			case SILENCER_STATE_POSE:
				creature->MaxTurn = 0;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
					item->Animation.RequiredState = SILENCER_STATE_AIM_1;
				}
				else
				{
					if (creature->Mood == MoodType::Attack || Random::TestProbability(1 / 128.0f))
						item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;

					if (!ai.ahead)
						item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
				}

				break;

			case SILENCER_STATE_AIM_1:
			case SILENCER_STATE_AIM_2:
				creature->MaxTurn = 0;
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}
				else
				{
					extraHeadRot.y = ai.angle;
				}

				if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
				}
				else if (Targetable(item, &ai))
				{
					item->Animation.TargetState = (item->Animation.ActiveState != SILENCER_STATE_AIM_1) ? SILENCER_STATE_SHOOT_2 : SILENCER_STATE_SHOOT_1;
				}
				else
				{
					item->Animation.TargetState = SILENCER_STATE_IDLE_FRAME;
				}

				break;

			case SILENCER_STATE_SHOOT_1:
			case SILENCER_STATE_SHOOT_2:
				creature->MaxTurn = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}
				else
				{
					extraHeadRot.y = ai.angle;
				}

				if (creature->Flags == 0 && item->Animation.FrameNumber == 0)
				{
					ShotLara(item, &ai, SilencerGunBite, extraTorsoRot.y, SILENCER_SHOOT_ATTACK_DAMAGE);
					creature->MuzzleFlash[0].Bite = SilencerGunBite;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags = 1;
				}

				break;

			case SILENCER_STATE_RUN_SHOOT:
				creature->MaxTurn = SILENCER_RUN_TURN_RATE_MAX;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}
				else
				{
					extraHeadRot.y = ai.angle;
				}

				if (item->Animation.RequiredState == NO_VALUE &&
					(item->Animation.AnimNumber == SILENCER_ANIM_RUN_FORWARD_SHOOT_LEFT &&
						item->Animation.FrameNumber == 1 ||
					item->Animation.AnimNumber == SILENCER_ANIM_RUN_FORWARD_SHOOT_RIGHT &&
						item->Animation.FrameNumber == 3))
				{
					if (!ShotLara(item, &ai, SilencerGunBite, extraTorsoRot.y, SILENCER_SHOOT_ATTACK_DAMAGE))
						item->Animation.TargetState = SILENCER_STATE_RUN_FORWARD;

					creature->MuzzleFlash[0].Bite = SilencerGunBite;
					creature->MuzzleFlash[0].Delay = 2;
					item->Animation.RequiredState = SILENCER_STATE_RUN_SHOOT;
				}

				break;
			}
		}

		CreatureTilt(item, tiltAngle);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);
	}
}
