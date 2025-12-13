#include "framework.h"
#include "Objects/TR1/Entity/Cowboy.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Renderer/Renderer.h"
#include "Specific/level.h"

using namespace TEN::Renderer;

namespace TEN::Entities::Creatures::TR1
{
	constexpr auto COWBOY_SHOT_DAMAGE = 70;
	constexpr auto COWBOY_WALK_RANGE = SQUARE(BLOCK(3));

	constexpr auto COWBOY_WALK_TURN_RATE_MAX = ANGLE(3.0f);
	constexpr auto COWBOY_RUN_TURN_RATE_MAX	 = ANGLE(6.0f);

	const auto CowboyGunLeft  = CreatureBiteInfo(Vector3(1.0f, 200.0f, 40.0f), 5);
	const auto CowboyGunRight = CreatureBiteInfo(Vector3(-1.0f, 200.0f, 40.0f), 8);

	enum CowboyState
	{
		// No state 0.
		COWBOY_STATE_IDLE = 1,
		COWBOY_STATE_WALK,
		COWBOY_STATE_RUN,
		COWBOY_STATE_AIM,
		COWBOY_STATE_DEATH,
		COWBOY_STATE_SHOOT
	};

	enum CowboyAnim
	{
		COWBOY_ANIM_RUN,
		COWBOY_ANIM_RUN_TO_IDLE_START,
		COWBOY_ANIM_RUN_TO_IDLE_END,
		COWBOY_ANIM_IDLE_TO_AIM,
		COWBOY_ANIM_SHOOT,
		COWBOY_ANIM_AIM,
		COWBOY_ANIM_AIM_TO_IDLE,
		COWBOY_ANIM_DEATH,
		COWBOY_ANIM_WALK,
		COWBOY_ANIM_WALK_TO_IDLE_START,
		COWBOY_ANIM_WALK_TO_IDLE_END,
		COWBOY_ANIM_IDLE_TO_WALK_START,
		COWBOY_ANIM_IDLE_TO_WALK_END,
		COWBOY_ANIM_IDLE_TO_RUN,
		COWBOY_ANIM_IDLE
	};

	void InitializeCowboy(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, COWBOY_ANIM_IDLE);
	}

	void CowboyControl(short itemNumber)
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
		if (creature.MuzzleFlash[1].Delay != 0)
			creature.MuzzleFlash[1].Delay--;

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != COWBOY_STATE_DEATH)
				SetAnimation(item, COWBOY_ANIM_DEATH);
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(&item, &ai);
			GetCreatureMood(&item, &ai, false);
			CreatureMood(&item, &ai, false);

			if (ai.ahead)
			{
				extraHeadRot.x = ai.xAngle;
				extraHeadRot.y = ai.angle;
			}

			headingAngle = CreatureTurn(&item, creature.MaxTurn);

			switch (item.Animation.ActiveState)
			{
			case COWBOY_STATE_IDLE:
				creature.MaxTurn = 0;

				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (Targetable(&item, &ai))
				{
					item.Animation.TargetState = COWBOY_STATE_AIM;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item.Animation.TargetState = COWBOY_STATE_WALK;
				}
				else
				{
					item.Animation.TargetState = COWBOY_STATE_RUN;
				}

				break;

			case COWBOY_STATE_WALK:
				creature.MaxTurn = COWBOY_WALK_TURN_RATE_MAX;

				if (creature.Mood == MoodType::Escape || !ai.ahead || ai.distance > COWBOY_WALK_RANGE)
				{
					item.Animation.TargetState = COWBOY_STATE_IDLE;
					item.Animation.RequiredState = COWBOY_STATE_RUN;
				}
				else if (Targetable(&item, &ai))
				{
					item.Animation.TargetState = COWBOY_STATE_IDLE;
					item.Animation.RequiredState = COWBOY_STATE_AIM;
				}

				break;

			case COWBOY_STATE_RUN:
				creature.MaxTurn = COWBOY_RUN_TURN_RATE_MAX;
				tiltAngle = headingAngle / 2;

				if (creature.Mood == MoodType::Escape && !ai.ahead)
					break;

				if (Targetable(&item, &ai))
				{
					item.Animation.TargetState = COWBOY_STATE_IDLE;
					item.Animation.RequiredState = COWBOY_STATE_AIM;
				}
				else if (ai.ahead && ai.distance < COWBOY_WALK_RANGE)
				{
					item.Animation.TargetState = COWBOY_STATE_IDLE;
					item.Animation.RequiredState = COWBOY_STATE_WALK;
				}

				break;

			case COWBOY_STATE_AIM:
				creature.MaxTurn = 0;
				creature.Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle / 2;
					extraTorsoRot.y = ai.angle / 2;
				}

				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = COWBOY_STATE_IDLE;
				}
				else if (Targetable(&item, &ai))
				{
					item.Animation.TargetState = COWBOY_STATE_SHOOT;
				}
				else
				{
					item.Animation.TargetState = COWBOY_STATE_IDLE;
				}

				break;

			case COWBOY_STATE_SHOOT:
				creature.MaxTurn = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle / 2;
					extraTorsoRot.y = ai.angle / 2;
				}

				if (Targetable(&item, &ai))
				{
					// TODO: Add gunflash for cowboy after PR #1069 is merged to develop branch.

					if (!(creature.Flags & 1) && item.Animation.FrameNumber == 2)
					{
						ShotLara(&item, &ai, CowboyGunLeft, extraHeadRot.y, COWBOY_SHOT_DAMAGE);
						creature.MuzzleFlash[0].Bite = CowboyGunLeft;
						creature.MuzzleFlash[0].Delay = 2;
						creature.Flags |= 1;
					}

					if (!(creature.Flags & 2) && item.Animation.FrameNumber == 10)
					{
						ShotLara(&item, &ai, CowboyGunRight, extraHeadRot.y, COWBOY_SHOT_DAMAGE);
						creature.MuzzleFlash[1].Bite = CowboyGunRight;
						creature.MuzzleFlash[1].Delay = 2;
						creature.Flags |= 2;
					}
				}

				if (creature.Mood == MoodType::Escape)
					item.Animation.RequiredState = COWBOY_STATE_RUN;

				break;
			}
		}

		CreatureTilt(&item, tiltAngle);
		CreatureJoint(&item, 0, extraHeadRot.y);
		CreatureJoint(&item, 1, extraHeadRot.x);
		CreatureJoint(&item, 2, extraTorsoRot.y);
		CreatureJoint(&item, 3, extraTorsoRot.x);
		CreatureAnimation(itemNumber, headingAngle, tiltAngle);
	}
}
