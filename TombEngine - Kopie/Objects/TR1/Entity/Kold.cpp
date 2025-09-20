#include "framework.h"
#include "Objects/TR1/Entity/Kold.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/misc.h"
#include "Game/people.h"

namespace TEN::Entities::Creatures::TR1
{
	constexpr auto KOLD_SHOT_DAMAGE = 150;

	constexpr auto KOLD_WALK_RANGE = SQUARE(BLOCK(4));

	constexpr auto KOLD_WALK_TURN_RATE_MAX = ANGLE(3.0f);
	constexpr auto KOLD_RUN_TURN_RATE_MAX  = ANGLE(6.0f);

	constexpr auto KOLD_SHOTGUN_SHELL_COUNT = 6;

	const auto KoldGunBite = CreatureBiteInfo(Vector3(-20, 440, 20), 9);

	enum KoldState
	{
		KOLD_STATE_DEATH = 0,
		KOLD_STATE_IDLE = 1,
		KOLD_STATE_WALK = 2,
		KOLD_STATE_RUN = 3,
		KOLD_STATE_AIM = 4,
		KOLD_STATE_SHOOT = 6
	};

	enum KoldAnim
	{
		KOLD_ANIM_RUN_TO_IDLE_START = 0,
		KOLD_ANIM_RUN_TO_IDLE_END = 1,
		KOLD_ANIM_AIM_START = 2,
		KOLD_ANIM_IDLE_TO_RUN_START = 3,
		KOLD_ANIM_IDLE_TO_RUN_END = 4,
		KOLD_ANIM_AIM_END = 5,
		KOLD_ANIM_UNAIM = 6,
		KOLD_ANIM_IDLE = 7,
		KOLD_ANIM_RUN = 8,
		KOLD_ANIM_WALK = 9,
		KOLD_ANIM_RELOAD = 10,
		KOLD_ANIM_SHOOT = 11,
		KOLD_ANIM_IDLE_TO_WALK = 12,
		KOLD_ANIM_WALK_TO_IDLE = 13,
		KOLD_ANIM_DEATH = 14
	};

	void InitializeKold(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, KOLD_ANIM_IDLE);
	}

	void ControlKold(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		if (creature.MuzzleFlash[0].Delay != 0)
			creature.MuzzleFlash[0].Delay--;

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != KOLD_STATE_DEATH)
				SetAnimation(item, KOLD_ANIM_DEATH);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			if (ai.ahead)
			{
				extraHeadRot.y = ai.angle / 2;
				extraTorsoRot.x = ai.xAngle / 2;
				extraTorsoRot.y = ai.angle / 2;
			}

			GetCreatureMood(&item, &ai, true);
			CreatureMood(&item, &ai, true);
			headingAngle = CreatureTurn(&item, creature.MaxTurn);

			switch (item.Animation.ActiveState)
			{
			case KOLD_STATE_IDLE:
				creature.MaxTurn = 0;

				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (Targetable(&item, &ai))
				{
					item.Animation.TargetState = KOLD_STATE_AIM;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item.Animation.TargetState = KOLD_STATE_WALK;
				}
				else
				{
					item.Animation.TargetState = KOLD_STATE_RUN;
				}

				break;

			case KOLD_STATE_WALK:
				creature.MaxTurn = KOLD_WALK_TURN_RATE_MAX;

				if (creature.Mood == MoodType::Escape || !ai.ahead)
				{
					item.Animation.TargetState = KOLD_STATE_IDLE;
					item.Animation.RequiredState = KOLD_STATE_RUN;
				}
				else if (Targetable(&item, &ai))
				{
					item.Animation.TargetState = KOLD_STATE_IDLE;
					item.Animation.RequiredState = KOLD_STATE_AIM;
				}
				else if (ai.distance > KOLD_WALK_RANGE)
				{
					item.Animation.TargetState = KOLD_STATE_IDLE;
					item.Animation.RequiredState = KOLD_STATE_RUN;
				}

				break;

			case KOLD_STATE_RUN:
				creature.MaxTurn = KOLD_RUN_TURN_RATE_MAX;
				tiltAngle = headingAngle / 2;

				if (creature.Mood == MoodType::Escape && !ai.ahead)
					break;

				if (Targetable(&item, &ai))
				{
					item.Animation.TargetState = KOLD_STATE_IDLE;
					item.Animation.RequiredState = KOLD_STATE_AIM;
				}
				else if (ai.ahead && ai.distance < KOLD_WALK_RANGE)
				{
					item.Animation.TargetState = KOLD_STATE_IDLE;
					item.Animation.RequiredState = KOLD_STATE_WALK;
				}

				break;

			case KOLD_STATE_AIM:
				creature.MaxTurn = 0;
				creature.Flags = 0;

				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = KOLD_STATE_IDLE;
				}
				else if (Targetable(&item, &ai))
				{
					item.Animation.TargetState = KOLD_STATE_SHOOT;
				}
				else
				{
					item.Animation.TargetState = KOLD_STATE_IDLE;
				}

				break;

			case KOLD_STATE_SHOOT:
				creature.MaxTurn = 0;

				if (creature.Flags == 0)
				{
					creature.MuzzleFlash[0].Bite = KoldGunBite;
					creature.MuzzleFlash[0].Delay = 2;
					ShotLara(&item, &ai, KoldGunBite, extraHeadRot.y, KOLD_SHOT_DAMAGE);
					creature.Flags = 1; // NOTE: Flag 1 = is attacking.
				}

				if (creature.Mood == MoodType::Escape)
					item.Animation.RequiredState = KOLD_STATE_RUN;

				break;
			}
		}

		CreatureTilt(&item, tiltAngle);
		CreatureJoint(&item, 0, extraHeadRot.y);
		CreatureJoint(&item, 1, extraTorsoRot.x);
		CreatureJoint(&item, 2, extraTorsoRot.y);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);
	}
}
