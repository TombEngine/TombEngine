#include "framework.h"
#include "Objects/TR3/Entity/Mercenary.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Game/control/lot.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto MERCENARY_SHOT_DAMAGE = 28;

	constexpr auto MERCENARY_RUN_RANGE		= SQUARE(BLOCK(2));
	constexpr auto MERCENARY_SHOOT1_RANGE	= SQUARE(BLOCK(3));
	constexpr auto MERCENARY_AWARE_DISTANCE = SQUARE(BLOCK(1));

	constexpr auto MERCENARY_WALK_TURN_RATE_MAX = ANGLE(5.0f);
	constexpr auto MERCENARY_RUN_TURN_RATE_MAX  = ANGLE(10.0f);

	constexpr auto MERCENARY_DIE_ANIM		= 19;
	constexpr auto MERCENARY_STOP_ANIM		= 12;
	constexpr auto MERCENARY_WALK_STOP_ANIM = 17;

	const auto MercenaryGunBite = CreatureBiteInfo(Vector3(0, 300, 64), 7);

	// Mercenaries don't attack these targets
	const auto MercenaryExcludedTargets = std::vector<GAME_OBJECT_ID>
	{
		ID_LARA,
		ID_MERCENARY
	};

	enum MercenaryState
	{
		MERCENARY_STATE_EMPTY = 0,
		MERCENARY_STATE_STOP = 1,
		MERCENARY_STATE_WALK = 2,
		MERCENARY_STATE_RUN = 3,
		MERCENARY_STATE_WAIT = 4,
		MERCENARY_STATE_SHOOT1 = 5,
		MERCENARY_STATE_SHOOT2 = 6,
		MERCENARY_STATE_DEATH = 7,
		MERCENARY_STATE_AIM1 = 8,
		MERCENARY_STATE_AIM2 = 9,
		MERCENARY_STATE_AIM3 = 10,
		MERCENARY_STATE_SHOOT3 = 11
	};

	// Find nearest enemy target for mercenary (excludes Lara and other mercenaries)
	static ItemInfo* FindMercenaryTarget(ItemInfo& item, const std::vector<GAME_OBJECT_ID>& excludedTargets)
	{
		float nearestDistance = FLT_MAX;
		ItemInfo* result = nullptr;

		for (auto creatureIndex : ActiveCreatures)
		{
			auto* targetCreature = GetCreatureInfo(&g_Level.Items[creatureIndex]);

			if (targetCreature->ItemNumber == NO_VALUE || targetCreature->ItemNumber == item.Index)
				continue;

			auto& currentItem = g_Level.Items[targetCreature->ItemNumber];

			if (currentItem.HitPoints <= 0)
				continue;

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
			if (distance < nearestDistance)
			{
				nearestDistance = distance;
				result = &currentItem;
			}
		}

		return result;
	}

	void InitializeMercenary(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, MERCENARY_STOP_ANIM);
	}

	void ControlMercenary(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short head = 0;
		auto extraTorsoRot = EulerAngles::Identity;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != MERCENARY_STATE_DEATH)
			{
				SetAnimation(*item, MERCENARY_DIE_ANIM);

				if (Random::TestProbability(0.25f))
					creature->Flags = 1;
				else
					creature->Flags = 0;
			}
		}
		else
		{
			if (item->AIBits)
			{
				GetAITarget(creature);
			}
			else if (creature->HurtByLara)
			{
				creature->Enemy = LaraItem;
			}
			else
			{
				// Find nearest enemy target (not Lara, not other mercenaries)
				TargetNearestEntity(*item, MercenaryExcludedTargets);
			}

			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			AI_INFO laraAI;
			if (creature->Enemy == LaraItem)
			{
				laraAI.angle = ai.angle;
				laraAI.distance = ai.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				laraAI.distance = SQUARE(dx) + SQUARE(dz);
			}

			// Don't target Lara if not hurt by her
			if (!creature->HurtByLara && creature->Enemy == LaraItem)
				creature->Enemy = nullptr;

			bool metaMood = (creature->Enemy != LaraItem);
			GetCreatureMood(item, &ai, metaMood);
			CreatureMood(item, &ai, metaMood);

			angle = CreatureTurn(item, creature->MaxTurn);

			// Alert guards only when hit
			if (item->HitStatus)
			{
				if (!creature->Alerted)
					SoundEffect(SFX_TR3_AMERCAN_HOY, &item->Pose);

				AlertAllGuards(itemNumber);
			}

			switch (item->Animation.ActiveState)
			{
			case MERCENARY_STATE_STOP:
				head = laraAI.angle;
				creature->Flags = 0;
				creature->MaxTurn = 0;

				if (item->Animation.AnimNumber == MERCENARY_WALK_STOP_ANIM)
				{
					if (abs(ai.angle) < MERCENARY_RUN_TURN_RATE_MAX)
						item->Pose.Orientation.y += ai.angle;
					else if (ai.angle < 0)
						item->Pose.Orientation.y -= MERCENARY_RUN_TURN_RATE_MAX;
					else
						item->Pose.Orientation.y += MERCENARY_RUN_TURN_RATE_MAX;
				}

				if (item->AIBits & GUARD)
				{
					head = AIGuard(creature);
					if (Random::TestProbability(1 / 256.0f))
					{
						if (item->Animation.ActiveState == MERCENARY_STATE_STOP)
							item->Animation.TargetState = MERCENARY_STATE_WAIT;
						else
							item->Animation.TargetState = MERCENARY_STATE_STOP;
					}
				}
				else if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = MERCENARY_STATE_WALK;
					head = 0;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = MERCENARY_STATE_RUN;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < MERCENARY_SHOOT1_RANGE || ai.zoneNumber != ai.enemyZone)
					{
						if (Random::TestProbability(0.5f))
							item->Animation.TargetState = MERCENARY_STATE_AIM1;
						else
							item->Animation.TargetState = MERCENARY_STATE_AIM3;
					}
					else
					{
						item->Animation.TargetState = MERCENARY_STATE_WALK;
					}
				}
				else if ((!creature->Alerted && creature->Mood == MoodType::Bored) ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					item->Animation.TargetState = MERCENARY_STATE_STOP;
				}
				else if (creature->Mood != MoodType::Bored && ai.distance > MERCENARY_RUN_RANGE)
				{
					item->Animation.TargetState = MERCENARY_STATE_RUN;
				}
				else
				{
					item->Animation.TargetState = MERCENARY_STATE_WALK;
				}

				break;

			case MERCENARY_STATE_WAIT:
				head = laraAI.angle;
				creature->Flags = 0;
				creature->MaxTurn = 0;

				if (item->AIBits & GUARD)
				{
					head = AIGuard(creature);
					if (Random::TestProbability(1 / 256.0f))
						item->Animation.TargetState = MERCENARY_STATE_STOP;
				}
				else if (Targetable(item, &ai))
				{
					item->Animation.TargetState = MERCENARY_STATE_SHOOT1;
				}
				else if (creature->Mood != MoodType::Bored || !ai.ahead)
				{
					item->Animation.TargetState = MERCENARY_STATE_STOP;
				}

				break;

			case MERCENARY_STATE_WALK:
				head = laraAI.angle;
				creature->Flags = 0;
				creature->MaxTurn = MERCENARY_WALK_TURN_RATE_MAX;

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = MERCENARY_STATE_WALK;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = MERCENARY_STATE_RUN;
				}
				else if ((item->AIBits & GUARD) ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					item->Animation.TargetState = MERCENARY_STATE_STOP;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance < MERCENARY_SHOOT1_RANGE || ai.zoneNumber != ai.enemyZone)
						item->Animation.TargetState = MERCENARY_STATE_STOP;
					else
						item->Animation.TargetState = MERCENARY_STATE_AIM2;
				}
				else if (creature->Mood == MoodType::Bored && ai.ahead)
				{
					item->Animation.TargetState = MERCENARY_STATE_STOP;
				}
				else if (creature->Mood != MoodType::Bored && ai.distance > MERCENARY_RUN_RANGE)
				{
					item->Animation.TargetState = MERCENARY_STATE_RUN;
				}

				break;

			case MERCENARY_STATE_RUN:
				if (ai.ahead)
					head = ai.angle;

				creature->MaxTurn = MERCENARY_RUN_TURN_RATE_MAX;
				tilt = angle / 2;

				if ((item->AIBits & GUARD) ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					item->Animation.TargetState = MERCENARY_STATE_WALK;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					break;
				}
				else if (Targetable(item, &ai))
				{
					item->Animation.TargetState = MERCENARY_STATE_WALK;
				}
				else if (creature->Mood == MoodType::Bored ||
					(creature->Mood == MoodType::Stalk && !(item->AIBits & FOLLOW) && ai.distance < MERCENARY_RUN_RANGE))
				{
					item->Animation.TargetState = MERCENARY_STATE_WALK;
				}

				break;

			case MERCENARY_STATE_AIM1:
			case MERCENARY_STATE_AIM3:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;

					if (Targetable(item, &ai))
					{
						item->Animation.TargetState = (item->Animation.ActiveState == MERCENARY_STATE_AIM1) ?
							MERCENARY_STATE_SHOOT1 : MERCENARY_STATE_SHOOT3;
					}
					else
					{
						item->Animation.TargetState = MERCENARY_STATE_STOP;
					}
				}

				break;

			case MERCENARY_STATE_AIM2:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;

					if (Targetable(item, &ai))
						item->Animation.TargetState = MERCENARY_STATE_SHOOT2;
					else
						item->Animation.TargetState = MERCENARY_STATE_WALK;
				}

				break;

			case MERCENARY_STATE_SHOOT3:
				if (item->Animation.TargetState != MERCENARY_STATE_STOP)
				{
					if (creature->Mood == MoodType::Escape || ai.distance > MERCENARY_SHOOT1_RANGE || !Targetable(item, &ai))
						item->Animation.TargetState = MERCENARY_STATE_STOP;
				}

				[[fallthrough]];

			case MERCENARY_STATE_SHOOT2:
			case MERCENARY_STATE_SHOOT1:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				if (!creature->Flags)
				{
					ShotLara(item, &ai, MercenaryGunBite, extraTorsoRot.y, MERCENARY_SHOT_DAMAGE);
					creature->MuzzleFlash[0].Bite = MercenaryGunBite;
					creature->MuzzleFlash[0].Delay = 2;
					creature->Flags = 5;
				}
				else
				{
					creature->Flags--;
				}

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, head);
		CreatureAnimation(itemNumber, angle, 0);
	}
}
