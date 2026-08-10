#include "framework.h"
#include "Objects/TR3/Entity/HybridMutant.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto HYBRID_MUTANT_SLASH_DAMAGE = 100;
	constexpr auto HYBRID_MUTANT_KICK_DAMAGE  = 80;
	constexpr auto HYBRID_MUTANT_JUMP_DAMAGE  = 20;

	constexpr auto HYBRID_MUTANT_ATTACK_CLOSE_RANGE = SQUARE(BLOCK(1));
	constexpr auto HYBRID_MUTANT_ATTACK_RANGE		= SQUARE(BLOCK(2));
	constexpr auto HYBRID_MUTANT_WALK_ATTACK_RANGE	= SQUARE(BLOCK(1.33f));
	constexpr auto HYBRID_MUTANT_AWARE_RANGE		= SQUARE(BLOCK(1));

	constexpr auto HYBRID_MUTANT_WALK_TURN_RATE_MAX = ANGLE(3.0f);
	constexpr auto HYBRID_MUTANT_RUN_TURN_RATE_MAX  = ANGLE(6.0f);

	const auto HybridMutantBiteLeft	 = CreatureBiteInfo(Vector3(19, -13, 3), 7);
	const auto HybridMutantBiteRight = CreatureBiteInfo(Vector3(19, -13, 3), 14);

	enum HybridMutantState
	{
		HYBRID_MUTANT_STATE_EMPTY = 0,
		HYBRID_MUTANT_STATE_IDLE = 1,
		HYBRID_MUTANT_STATE_WALK = 2,
		HYBRID_MUTANT_STATE_RUN = 3,
		HYBRID_MUTANT_STATE_JUMP_START = 4,
		HYBRID_MUTANT_STATE_JUMP_MID = 5,
		HYBRID_MUTANT_STATE_JUMP_END = 6,
		HYBRID_MUTANT_STATE_SLASH_LEFT = 7,
		HYBRID_MUTANT_STATE_KICK = 8,
		HYBRID_MUTANT_STATE_RUN_ATTACK = 9,
		HYBRID_MUTANT_STATE_WALK_ATTACK = 10,
		HYBRID_MUTANT_STATE_DEATH = 11
	};

	enum HybridMutantAnim
	{
		HYBRID_MUTANT_ANIM_DEATH = 18
	};

	void ControlHybridMutant(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;
		auto headOrient = EulerAngles::Identity;
		auto torsoOrient = EulerAngles::Identity;

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != HYBRID_MUTANT_STATE_DEATH)
				SetAnimation(item, HYBRID_MUTANT_ANIM_DEATH);
		}
		else
		{
			if (item.AIBits)
				GetAITarget(&creature);

			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			AI_INFO laraAI;
			if (creature.Enemy == LaraItem)
			{
				laraAI.angle = ai.angle;
				laraAI.distance = ai.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item.Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item.Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item.Pose.Orientation.y;
				laraAI.distance = SQUARE(dx) + SQUARE(dz);
			}

			GetCreatureMood(&item, &ai, true);
			CreatureMood(&item, &ai, true);
			headingAngle = CreatureTurn(&item, creature.MaxTurn);

			auto* target = creature.Enemy.Get();
			creature.Enemy = LaraItem;
			if (laraAI.distance < HYBRID_MUTANT_AWARE_RANGE || item.HitStatus || TargetVisible(&item, &laraAI))
				AlertAllGuards(itemNumber);
			creature.Enemy = target;

			switch (item.Animation.ActiveState)
			{
			case HYBRID_MUTANT_STATE_IDLE:
				creature.MaxTurn = 0;
				creature.Flags = 0;
				headOrient.y = ai.angle;

				if (item.AIBits & GUARD)
				{
					headOrient.y = AIGuard(&creature);
					item.Animation.TargetState = HYBRID_MUTANT_STATE_IDLE;
				}
				else if (item.AIBits & PATROL1)
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_WALK;
					headOrient.y = 0;
				}
				else if (creature.Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != &item && ai.ahead)
						item.Animation.TargetState = HYBRID_MUTANT_STATE_IDLE;
					else
						item.Animation.TargetState = HYBRID_MUTANT_STATE_RUN;
				}
				else if (ai.angle < ANGLE(45.0f) && ai.angle > ANGLE(-45.0f) && ai.distance > HYBRID_MUTANT_ATTACK_CLOSE_RANGE)
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_JUMP_START;
				}
				else if (ai.bite && ai.distance < HYBRID_MUTANT_ATTACK_CLOSE_RANGE)
				{
					torsoOrient.x = ai.xAngle;
					torsoOrient.y = ai.angle;

					if (ai.angle < 0)
						item.Animation.TargetState = HYBRID_MUTANT_STATE_SLASH_LEFT;
					else
						item.Animation.TargetState = HYBRID_MUTANT_STATE_KICK;
				}
				else if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (ai.distance < HYBRID_MUTANT_ATTACK_RANGE)
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_WALK;
				}
				else
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_RUN;
				}

				break;

			case HYBRID_MUTANT_STATE_WALK:
				creature.MaxTurn = HYBRID_MUTANT_WALK_TURN_RATE_MAX;

				if (ai.ahead)
					headOrient.y = ai.angle;

				if (item.AIBits & PATROL1)
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_WALK;
					headOrient.y = 0;
				}
				else if (ai.bite && ai.distance < HYBRID_MUTANT_WALK_ATTACK_RANGE)
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_WALK_ATTACK;
				}
				else if (creature.Mood == MoodType::Escape || creature.Mood == MoodType::Attack)
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_RUN;
				}

				break;

			case HYBRID_MUTANT_STATE_RUN:
				creature.MaxTurn = HYBRID_MUTANT_RUN_TURN_RATE_MAX;

				if (ai.ahead)
					headOrient.y = ai.angle;

				if (item.AIBits & GUARD)
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Bored ||
					(creature.Mood == MoodType::Escape && Lara.TargetEntity != &item && ai.ahead))
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_IDLE;
				}
				else if (creature.Flags != 0 && ai.ahead)
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_IDLE;
				}
				else if (ai.bite && ai.distance < HYBRID_MUTANT_ATTACK_RANGE)
				{
					if (LaraItem->Animation.Velocity.z == 0.0f)
						item.Animation.TargetState = HYBRID_MUTANT_STATE_IDLE;
					else
						item.Animation.TargetState = HYBRID_MUTANT_STATE_RUN_ATTACK;
				}
				else if (ai.distance < HYBRID_MUTANT_ATTACK_RANGE)
				{
					item.Animation.TargetState = HYBRID_MUTANT_STATE_WALK;
				}

				creature.Flags = 0;
				break;

			case HYBRID_MUTANT_STATE_WALK_ATTACK:
			case HYBRID_MUTANT_STATE_RUN_ATTACK:
			case HYBRID_MUTANT_STATE_SLASH_LEFT:
				creature.MaxTurn = HYBRID_MUTANT_WALK_TURN_RATE_MAX;

				if (ai.ahead)
				{
					torsoOrient.x = ai.xAngle;
					torsoOrient.y = ai.angle;
				}

				if (creature.Flags == 0 && item.TouchBits.Test(HybridMutantBiteLeft.BoneID))
				{
					DoDamage(creature.Enemy, HYBRID_MUTANT_SLASH_DAMAGE);
					CreatureEffect2(&item, HybridMutantBiteLeft, 10, item.Pose.Orientation.y, DoBloodSplat);
					creature.Flags = 1;
				}

				if (!(ai.bite && ai.distance < HYBRID_MUTANT_ATTACK_CLOSE_RANGE))
					item.Animation.TargetState = HYBRID_MUTANT_STATE_IDLE;

				if (TestLastFrame(item))
					creature.Flags = 0;

				break;

			case HYBRID_MUTANT_STATE_KICK:
				creature.MaxTurn = HYBRID_MUTANT_WALK_TURN_RATE_MAX;

				if (ai.ahead)
				{
					torsoOrient.x = ai.xAngle;
					torsoOrient.y = ai.angle;
				}

				if (creature.Flags == 0 && item.TouchBits.Test(HybridMutantBiteRight.BoneID))
				{
					DoDamage(creature.Enemy, HYBRID_MUTANT_KICK_DAMAGE);
					CreatureEffect2(&item, HybridMutantBiteRight, 10, item.Pose.Orientation.y, DoBloodSplat);
					creature.Flags = 1;
				}

				if (!(ai.bite && ai.distance < HYBRID_MUTANT_ATTACK_CLOSE_RANGE))
					item.Animation.TargetState = HYBRID_MUTANT_STATE_IDLE;

				if (TestLastFrame(item))
					creature.Flags = 0;

				break;

			case HYBRID_MUTANT_STATE_JUMP_START:
				creature.MaxTurn = HYBRID_MUTANT_WALK_TURN_RATE_MAX;
				break;

			case HYBRID_MUTANT_STATE_JUMP_MID:
			case HYBRID_MUTANT_STATE_JUMP_END:
				creature.MaxTurn = 0;

				if (ai.ahead)
				{
					torsoOrient.x = ai.xAngle;
					torsoOrient.y = ai.angle;
				}

				if (item.TouchBits.Test(HybridMutantBiteLeft.BoneID))
				{
					DoDamage(creature.Enemy, HYBRID_MUTANT_JUMP_DAMAGE);
					CreatureEffect2(&item, HybridMutantBiteLeft, 10, item.Pose.Orientation.y, DoBloodSplat);
				}

				break;
			}
		}

		CreatureJoint(&item, 0, torsoOrient.x);
		CreatureJoint(&item, 1, torsoOrient.y);
		CreatureJoint(&item, 2, headOrient.y);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
