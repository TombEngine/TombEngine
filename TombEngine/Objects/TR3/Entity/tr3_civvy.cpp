#include "framework.h"
#include "Objects/TR3/Entity/tr3_civvy.h"

#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto CIVVY_ATTACK_DAMAGE = 50;

	constexpr auto CIVVY_WALK_RANGE				  = SQUARE(BLOCK(2));
	constexpr auto CIVVY_ESCAPE_RANGE			  = SQUARE(BLOCK(5));
	constexpr auto CIVVY_ALERT_RANGE			  = SQUARE(BLOCK(15));
	constexpr auto CIVVY_ATTACK_WALK_PUNCH_RANGE  = SQUARE(BLOCK(0.8f));
	constexpr auto CIVVY_ATTACK_CLOSE_PUNCH_RANGE = SQUARE(BLOCK(0.5f));
	constexpr auto CIVVY_ATTACK_FAR_PUNCH_RANGE	  = SQUARE(BLOCK(0.7f));

	constexpr auto CIVVY_WAIT_CHANCE = 1.0f / 128;

	constexpr auto CIVVY_WALK_TURN_RATE_MAX = ANGLE(5.0f);
	constexpr auto CIVVY_RUN_TURN_RATE_MAX	= ANGLE(6.0f);
	constexpr auto CIVVY_AIM_TURN_RATE_MAX	= ANGLE(8.0f);

	constexpr auto CIVVY_TARGET_ALERT_VELOCITY = 10.0f;
	constexpr auto CIVVY_VAULT_SHIFT = 260;

	const auto CivvyBiteLeft  = CreatureBiteInfo(Vector3::Zero, 10);
	const auto CivvyBiteRight = CreatureBiteInfo(Vector3::Zero, 13);
	const auto CivvyAttackJoints = std::vector<unsigned int>{ 10, 13 };
  
	const auto CivvyExcludedTargets = std::vector<GAME_OBJECT_ID>
	{
		ID_CIVVY,
		ID_VON_CROY,
		ID_GUIDE,
		ID_MONK1,
		ID_MONK2,
		ID_TROOPS
	};

	enum CivvyState
	{
		// No state 0.
		CIVVY_STATE_IDLE = 1,
		CIVVY_STATE_WALK = 2,
		CIVVY_STATE_ATTACK_WALKING_PUNCH = 3, // Punch while walking.
		CIVVY_STATE_AIM_WALKING_PUNCH = 4,
		CIVVY_STATE_WAIT = 5,
		CIVVY_STATE_AIM_FAR_PUNCH = 6,		  // Punch from idle to walk.
		CIVVY_STATE_AIM_CLOSE_PUNCH = 7,	  // Punch while idle.
		CIVVY_STATE_ATTACK_FAR_PUNCH = 8,
		CIVVY_STATE_ATTACK_CLOSE_PUNCH = 9,
		CIVVY_STATE_RUN = 10,
		CIVVY_STATE_DEATH = 11,
		CIVVY_STATE_CLIMB_2CLICKS = 12,
		CIVVY_STATE_CLIMB_3CLICKS = 13,
		CIVVY_STATE_CLIMB_4CLICKS = 14,
		CIVVY_STATE_FALL_4CLICKS = 15
	};

	enum CivvyAnim
	{
		CIVVY_ANIM_WALK = 0,
		CIVVY_ANIM_WALKING_PUNCH_PREPARE = 1,
		CIVVY_ANIM_WALKING_PUNCH_TO_IDLE = 2,
		CIVVY_ANIM_WALKING_PUNCH_TO_WALK = 3,
		CIVVY_ANIM_WALKING_PUNCH_ATTACK = 4,
		CIVVY_ANIM_IDLE_TO_WALK = 5,
		CIVVY_ANIM_IDLE = 6,
		CIVVY_ANIM_WALKING_PUNCH_CANCEL = 7,
		CIVVY_ANIM_WALK_TO_IDLE_LEFT = 8,
		CIVVY_ANIM_CLOSE_PUNCH_CANCEL = 9,
		CIVVY_ANIM_CLOSE_PUNCH_PREPARE = 10,
		CIVVY_ANIM_FAR_PUNCH_CANCEL = 11,
		CIVVY_ANIM_FAR_PUNCH_PREPARE = 12,
		CIVVY_ANIM_CLOSE_PUNCH_TO_IDLE = 13,
		CIVVY_ANIM_FAR_PUNCH_TO_IDLE = 14,
		CIVVY_ANIM_FAR_PUNCH_TO_WALK = 15,
		CIVVY_ANIM_CLOSE_PUNCH_ATTACK = 16,
		CIVVY_ANIM_FAR_PUNCH_ATTACK = 17,
		CIVVY_ANIM_IDLE_TO_WAIT = 18,
		CIVVY_ANIM_WAIT_TO_IDLE = 19,
		CIVVY_ANIM_WAIT = 20,
		CIVVY_ANIM_RUN = 21,
		CIVVY_ANIM_WALK_TO_RUN = 22,
		CIVVY_ANIM_RUN_TO_WALK_LEFT = 23,
		CIVVY_ANIM_RUN_TO_IDLE = 24,
		CIVVY_ANIM_IDLE_TO_RUN = 25,
		CIVVY_ANIM_DEATH = 26,
		CIVVY_ANIM_VAULT_4_STEPS_UP = 27,
		CIVVY_ANIM_VAULT_2_STEPS_UP = 28,
		CIVVY_ANIM_VAULT_3_STEPS_UP = 29,
		CIVVY_ANIM_VAULT_4_STEPS_DOWN = 30,
		CIVVY_ANIM_RUN_TO_WALK_RIGHT = 31
	};

	// TODO: In future damage type refactor, create enum for flags used in all enemies with melee attacks.
	enum CivvyCreatureFlags
	{
		CIVVY_HAS_NOT_HIT_YET = 0,
		CIVVY_HAS_ALREADY_HIT = 1,
		CIVVY_IS_DOING_HIT = 2
	};

	// If this function works well, in the future it could be made generic for other other ally entities.
	static ItemInfo& FindNearestCivvyTarget(ItemInfo& item, const std::vector<GAME_OBJECT_ID>& excludedTargets, float rangeDetection)
	{
		float maxRange = (rangeDetection <= 0) ? INFINITY : rangeDetection;

		float nearestDistance = INFINITY;
		ItemInfo* result = nullptr;
		for (auto& targetCreature : ActiveCreatures)
		{
			// Ignore itself and invalid entities.
			if (targetCreature->ItemNumber == NO_VALUE || targetCreature->ItemNumber == item.Index)
				continue;

			auto& currentItem = g_Level.Items[targetCreature->ItemNumber];

			// Ignore if dead.
			if (currentItem.HitPoints <= 0)
				continue;

			// Ignore if entity is in excluded targets list.
			bool isForbiddenTarget = false;
			for (const auto& excludedTargetID : excludedTargets)
			{
				if (currentItem.ObjectNumber == excludedTargetID)
				{
					isForbiddenTarget = true;
					break;
				}
			}

			if (isForbiddenTarget)
				continue;

			float distance = Vector3i::Distance(item.Pose.Position, currentItem.Pose.Position);
			if (distance < nearestDistance && distance < maxRange)
			{
				nearestDistance = distance;
				result = &currentItem;
			}
		}

		return *result;
	}

	void InitializeCivvy(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, CIVVY_ANIM_IDLE);
	}

	void CivvyControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short angle = 0;
		short tilt = 0;

		int targetAngle = 0;
		int targetDistance = 0;
		auto jointHeadRot = EulerAngles::Identity;
		auto jointTorsoRot = EulerAngles::Identity;

		if (item.BoxNumber != NO_VALUE && (g_Level.PathfindingBoxes[item.BoxNumber].flags & BLOCKED) && item.HitPoints > 0)
		{
			DoDamage(&item, INT_MAX);
			DoLotsOfBlood(item.Pose.Position.x, item.Pose.Position.y - (GetRandomControl() & 255) - 32, item.Pose.Position.z, (GetRandomControl() & 127) + 128, GetRandomControl() << 1, item.RoomNumber, 3);
		}

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != CIVVY_STATE_DEATH)
				SetAnimation(item, CIVVY_ANIM_DEATH);
		}
		else
		{
			if (item.AIBits)
				GetAITarget(&creature);
			else
				creature.Enemy = &FindNearestCivvyTarget(item, CivvyExcludedTargets, CIVVY_ALERT_RANGE);

			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			if (creature.Enemy->IsLara())
			{
				targetAngle = ai.angle;
				targetDistance = ai.distance;
			}
			else
			{
				int targetDx = creature.Enemy->Pose.Position.x - item.Pose.Position.x;
				int targetDz = creature.Enemy->Pose.Position.z - item.Pose.Position.z;
				targetAngle = phd_atan(targetDz, targetDx) - item.Pose.Orientation.y;
				targetDistance = SQUARE(targetDx) + SQUARE(targetDz);
			}

			//If Lara was placed by system (CreatureAIInfo), not because she were a real target. Then delete the target.
			if (creature.Enemy->IsLara() && !creature.HurtByLara)
				creature.Enemy = nullptr;

			GetCreatureMood(&item, &ai, true);

			if (creature.Enemy == LaraItem &&
				ai.distance > CIVVY_ESCAPE_RANGE &&
				ai.enemyFacing < ANGLE(67.0f) &&
				ai.enemyFacing > -ANGLE(67.0f))
			{
				creature.Mood = MoodType::Escape;
			}

			CreatureMood(&item, &ai, true);

			int angle = CreatureTurn(&item, creature.MaxTurn);

			// TODO: Find way to detect entities with AI object (issue exists for other entities too).
			if (item.HitStatus ||
				(targetDistance <= CIVVY_ALERT_RANGE &&
					!(item.AIBits & FOLLOW)))
			{
				if (!creature.Alerted)
					SoundEffect(SFX_TR3_AMERCAN_HOY, &item.Pose);

				AlertAllGuards(itemNumber);
			}

			switch (item.Animation.ActiveState)
			{
			case CIVVY_STATE_IDLE:
				creature.MaxTurn = 0;

				if (ai.ahead)
					jointHeadRot.y = targetAngle;

				if (item.AIBits & GUARD)
				{
					jointHeadRot.y = AIGuard(&creature);

					if (!(GetRandomControl() & 0xFF))
					{
						if (item.Animation.ActiveState == CIVVY_STATE_IDLE)
						{
							item.Animation.TargetState = CIVVY_STATE_WAIT;
						}
						else
						{
							item.Animation.TargetState = CIVVY_STATE_IDLE;
						}
					}

					break;
				}
				else if (item.AIBits & PATROL1)
				{
					item.Animation.TargetState = CIVVY_STATE_WALK;
				}
				else if (creature.Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != &item && ai.ahead)
					{
						item.Animation.TargetState = CIVVY_STATE_IDLE;
					}
					else
					{
						item.Animation.TargetState = CIVVY_STATE_RUN;
					}
				}
				else if (creature.Mood == MoodType::Bored ||
					(item.AIBits & FOLLOW && (creature.ReachedGoal || targetDistance > SQUARE(BLOCK(2)))))
				{
					if (!creature.Alerted && Random::TestProbability(CIVVY_WAIT_CHANCE))
					{
						item.Animation.TargetState = CIVVY_STATE_WAIT;
					}
					else
					{
						item.Animation.TargetState = CIVVY_STATE_IDLE;
					}
				}
				else if (creature.Enemy != nullptr && ai.bite && ai.distance < CIVVY_ATTACK_CLOSE_PUNCH_RANGE)
				{
					item.Animation.TargetState = CIVVY_STATE_AIM_CLOSE_PUNCH;
				}
				else if (creature.Enemy != nullptr && ai.bite && ai.distance < CIVVY_ATTACK_FAR_PUNCH_RANGE)
				{
					item.Animation.TargetState = CIVVY_STATE_AIM_FAR_PUNCH;
				}
				else if (ai.bite && ai.distance < CIVVY_WALK_RANGE)
				{
					item.Animation.TargetState = CIVVY_STATE_WALK;
				}
				else
				{
					item.Animation.TargetState = CIVVY_STATE_RUN;
				}

				break;

			case CIVVY_STATE_WALK:
				creature.MaxTurn = CIVVY_WALK_TURN_RATE_MAX;

				if (ai.ahead)
					jointHeadRot.y = targetAngle;

				if (item.AIBits & PATROL1)
				{
					jointHeadRot.y = 0;
					item.Animation.TargetState = CIVVY_STATE_WALK;
				}
				else if (item.AIBits & FOLLOW)
				{
					if (creature.ReachedGoal)
					{
						item.Animation.TargetState = CIVVY_STATE_IDLE;
					}
					else if (targetDistance > CIVVY_WALK_RANGE)
					{
						item.Animation.TargetState = CIVVY_STATE_RUN;
					}
					else
					{
						item.Animation.TargetState = CIVVY_STATE_WALK;
					}
				}
				else if (creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = CIVVY_STATE_RUN;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					if (Random::TestProbability(CIVVY_WAIT_CHANCE))
						item.Animation.TargetState = CIVVY_STATE_IDLE;
				}
				else if (creature.Enemy != nullptr &&
					ai.distance < CIVVY_ATTACK_CLOSE_PUNCH_RANGE &&
					creature.Enemy->Animation.Velocity.z < CIVVY_TARGET_ALERT_VELOCITY)
				{
					item.Animation.TargetState = CIVVY_STATE_IDLE;
				}
				else if (creature.Enemy != nullptr && ai.bite && ai.distance < CIVVY_ATTACK_WALK_PUNCH_RANGE)
				{
					item.Animation.TargetState = CIVVY_STATE_AIM_WALKING_PUNCH;
				}
				else if (ai.distance > CIVVY_WALK_RANGE)
				{
					item.Animation.TargetState = CIVVY_STATE_RUN;
				}
				else
				{
					item.Animation.TargetState = CIVVY_STATE_WALK;
				}

				break;

			case CIVVY_STATE_WAIT:
				creature.MaxTurn = 0;

				if (ai.ahead)
				{
					jointHeadRot.y = targetAngle;
				}
				else if (item.AIBits & GUARD)
				{
					jointHeadRot.y = AIGuard(&creature);
				}

				if (creature.Alerted || Random::TestProbability(CIVVY_WAIT_CHANCE))
					item.Animation.TargetState = CIVVY_STATE_IDLE;

				break;

			case CIVVY_STATE_RUN:
				creature.MaxTurn = CIVVY_RUN_TURN_RATE_MAX;

				if (ai.ahead)
					tilt = angle / 2;

				if (ai.ahead)
					jointHeadRot.y = ai.angle;

				if (item.AIBits & GUARD)
				{
					item.Animation.TargetState = CIVVY_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = CIVVY_STATE_RUN;
				}
				else if ((item.AIBits & FOLLOW) && (creature.ReachedGoal || targetDistance > CIVVY_WALK_RANGE))
				{
					item.Animation.TargetState = CIVVY_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item.Animation.TargetState = CIVVY_STATE_WALK;
				}
				else if (ai.ahead && ai.distance <= CIVVY_WALK_RANGE)
				{
					item.Animation.TargetState = CIVVY_STATE_WALK;
				}

				break;

			case CIVVY_STATE_AIM_CLOSE_PUNCH:
				creature.MaxTurn = CIVVY_AIM_TURN_RATE_MAX;
				creature.Flags = CIVVY_HAS_NOT_HIT_YET;

				if (ai.ahead)
				{
					jointTorsoRot.x = ai.xAngle;
					jointTorsoRot.y = ai.angle;
				}

				if (creature.Enemy != nullptr && ai.bite && ai.distance < CIVVY_ATTACK_CLOSE_PUNCH_RANGE)
				{
					item.Animation.TargetState = CIVVY_STATE_ATTACK_CLOSE_PUNCH;
				}
				else
				{
					item.Animation.TargetState = CIVVY_STATE_IDLE;
				}

				break;

			case CIVVY_STATE_AIM_FAR_PUNCH:
				creature.MaxTurn = CIVVY_AIM_TURN_RATE_MAX;
				creature.Flags = CIVVY_HAS_NOT_HIT_YET;

				if (ai.ahead)
				{
					jointTorsoRot.x = ai.xAngle;
					jointTorsoRot.y = ai.angle;
				}

				if (creature.Enemy != nullptr && ai.bite && ai.distance < CIVVY_ATTACK_FAR_PUNCH_RANGE)
				{
					item.Animation.TargetState = CIVVY_STATE_ATTACK_FAR_PUNCH;
				}
				else
				{
					item.Animation.TargetState = CIVVY_STATE_IDLE;
				}

				break;

			case CIVVY_STATE_AIM_WALKING_PUNCH:
				creature.MaxTurn = CIVVY_AIM_TURN_RATE_MAX;
				creature.Flags = CIVVY_HAS_NOT_HIT_YET;

				if (ai.ahead)
				{
					jointTorsoRot.x = ai.xAngle;
					jointTorsoRot.y = ai.angle;
				}

				if (creature.Enemy != nullptr && ai.bite && ai.distance < CIVVY_ATTACK_WALK_PUNCH_RANGE)
				{
					item.Animation.TargetState = CIVVY_STATE_ATTACK_WALKING_PUNCH;
				}
				else
				{
					item.Animation.TargetState = CIVVY_STATE_WALK;
				}

				break;

			case CIVVY_STATE_ATTACK_CLOSE_PUNCH:
				creature.MaxTurn = CIVVY_WALK_TURN_RATE_MAX;

				if (ai.ahead)
				{
					jointTorsoRot.x = ai.xAngle;
					jointTorsoRot.y = ai.angle;
				}

				if (creature.Flags == CIVVY_HAS_NOT_HIT_YET)
				{
					if (creature.Enemy->IsLara())
					{
						if (item.TouchBits.Test(CivvyAttackJoints))
							creature.Flags = CIVVY_IS_DOING_HIT;
					}
					else
					{
						float distance = Vector3i::Distance(item.Pose.Position, creature.Enemy->Pose.Position);
						if (distance <= BLOCK(0.5))
							creature.Flags = CIVVY_IS_DOING_HIT;
					}

					if (creature.Flags == CIVVY_IS_DOING_HIT)
					{
						DoDamage(creature.Enemy, CIVVY_ATTACK_DAMAGE);
						CreatureEffect(&item, CivvyBiteLeft, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item.Pose);
						creature.Flags = CIVVY_HAS_ALREADY_HIT;

						if (creature.Enemy->HitPoints <= 0 && !creature.Enemy->IsLara() && !item.HitStatus)
							creature.Alerted = false;
					}
				}

				break;

			case CIVVY_STATE_ATTACK_FAR_PUNCH:
				creature.MaxTurn = CIVVY_WALK_TURN_RATE_MAX;

				if (ai.ahead)
				{
					jointTorsoRot.x = ai.xAngle;
					jointTorsoRot.y = ai.angle;
				}

				if (creature.Flags == CIVVY_HAS_NOT_HIT_YET)
				{
					if (creature.Enemy->IsLara())
					{
						if (item.TouchBits.Test(CivvyAttackJoints))
							creature.Flags = CIVVY_IS_DOING_HIT;
					}
					else
					{
						float distance = Vector3i::Distance(item.Pose.Position, creature.Enemy->Pose.Position);
						if (distance <= CLICK(2))
							creature.Flags = CIVVY_IS_DOING_HIT;
					}

					if (creature.Flags == CIVVY_IS_DOING_HIT)
					{
						DoDamage(creature.Enemy, CIVVY_ATTACK_DAMAGE);
						CreatureEffect(&item, CivvyBiteLeft, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item.Pose);
						creature.Flags = CIVVY_HAS_ALREADY_HIT;

						if (creature.Enemy->HitPoints <= 0 && !creature.Enemy->IsLara() && !item.HitStatus)
							creature.Alerted = false;
					}
				}

				if (ai.ahead && ai.distance > CIVVY_ATTACK_FAR_PUNCH_RANGE)
					item.Animation.TargetState = CIVVY_STATE_WALK;

				break;

			case CIVVY_STATE_ATTACK_WALKING_PUNCH:
				creature.MaxTurn = CIVVY_WALK_TURN_RATE_MAX;

				if (ai.ahead)
				{
					jointTorsoRot.x = ai.xAngle;
					jointTorsoRot.y = ai.angle;
				}

				if (creature.Flags == CIVVY_HAS_NOT_HIT_YET)
				{
					if (creature.Enemy->IsLara())
					{
						if (item.TouchBits.Test(CivvyAttackJoints))
							creature.Flags = CIVVY_IS_DOING_HIT;
					}
					else
					{
						float distance = Vector3i::Distance(item.Pose.Position, creature.Enemy->Pose.Position);
						if (distance <= CLICK(2))
							creature.Flags = CIVVY_IS_DOING_HIT;
					}

					if (creature.Flags == CIVVY_IS_DOING_HIT)
					{
						DoDamage(creature.Enemy, CIVVY_ATTACK_DAMAGE);
						CreatureEffect(&item, CivvyBiteLeft, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item.Pose);
						creature.Flags = CIVVY_HAS_ALREADY_HIT;

						if (creature.Enemy->HitPoints <= 0 && !creature.Enemy->IsLara() && !item.HitStatus)
							creature.Alerted = false;
					}
				}

				break;
			}
		}

		CreatureTilt(&item, tilt);
		CreatureJoint(&item, 0, jointTorsoRot.y);
		CreatureJoint(&item, 1, jointTorsoRot.x);
		CreatureJoint(&item, 2, jointHeadRot.y);

		if (item.Animation.ActiveState < CIVVY_STATE_DEATH)
		{
			switch (CreatureVault(itemNumber, angle, 2, CIVVY_VAULT_SHIFT))
			{
			case 2:
				creature.MaxTurn = 0;
				SetAnimation(item, CIVVY_ANIM_VAULT_2_STEPS_UP);
				break;

			case 3:
				creature.MaxTurn = 0;
				SetAnimation(item, CIVVY_ANIM_VAULT_3_STEPS_UP);
				break;

			case 4:
				creature.MaxTurn = 0;
				SetAnimation(item, CIVVY_ANIM_VAULT_4_STEPS_UP);
				break;

			case -4:
				creature.MaxTurn = 0;
				SetAnimation(item, CIVVY_ANIM_VAULT_4_STEPS_DOWN);
				break;
			}
		}
		else
		{
			CreatureAnimation(itemNumber, angle, 0);
		}
	}
}
