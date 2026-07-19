#include "framework.h"
#include "Objects/TR3/Entity/Prisoner.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto PRISONER_HIT_DAMAGE   = 40;
	constexpr auto PRISONER_SWIPE_DAMAGE = 50;

	constexpr auto PRISONER_ATTACK0_RANGE  = SQUARE(BLOCK(0.33f));
	constexpr auto PRISONER_ATTACK1_RANGE  = SQUARE(BLOCK(0.66f));
	constexpr auto PRISONER_ATTACK2_RANGE  = SQUARE(BLOCK(0.75f));
	constexpr auto PRISONER_WALK_RANGE	  = SQUARE(BLOCK(1));
	constexpr auto PRISONER_AWARE_DISTANCE = SQUARE(BLOCK(1));
	constexpr auto PRISONER_HIT_RADIUS	  = CLICK(1);

	constexpr auto PRISONER_WALK_TURN_RATE_MAX = ANGLE(7.0f);
	constexpr auto PRISONER_RUN_TURN_RATE_MAX  = ANGLE(11.0f);

	constexpr auto PRISONER_DIE_ANIM    = 26;
	constexpr auto PRISONER_STOP_ANIM   = 6;
	constexpr auto PRISONER_CLIMB1_ANIM = 28;
	constexpr auto PRISONER_CLIMB2_ANIM = 29;
	constexpr auto PRISONER_CLIMB3_ANIM = 27;
	constexpr auto PRISONER_FALL3_ANIM  = 30;

	constexpr auto PRISONER_VAULT_SHIFT = 260;
	constexpr auto PRISONER_TOUCH = 0x2400;

	const auto PrisonerHitBite = CreatureBiteInfo(Vector3(10, 10, 11), 13);
	const auto PrisonerAttackJoints = std::vector<unsigned int>{ 13 };

	// Prisoner doesn't attack these targets
	const auto PrisonerExcludedTargets = std::vector<GAME_OBJECT_ID>
	{
		ID_LARA,
		ID_CIVVY,
		ID_VON_CROY,
		ID_GUIDE,
		ID_MONK1,
		ID_MONK2,
		ID_TROOPS,
		ID_PRISONER,
		ID_PUNK
	};

	enum PrisonerState
	{
		PRISONER_STATE_EMPTY = 0,
		PRISONER_STATE_STOP = 1,
		PRISONER_STATE_WALK = 2,
		PRISONER_STATE_PUNCH2 = 3,
		PRISONER_STATE_AIM2 = 4,
		PRISONER_STATE_WAIT = 5,
		PRISONER_STATE_AIM1 = 6,
		PRISONER_STATE_AIM0 = 7,
		PRISONER_STATE_PUNCH1 = 8,
		PRISONER_STATE_PUNCH0 = 9,
		PRISONER_STATE_RUN = 10,
		PRISONER_STATE_DEATH = 11,
		PRISONER_STATE_CLIMB3 = 12,
		PRISONER_STATE_CLIMB1 = 13,
		PRISONER_STATE_CLIMB2 = 14,
		PRISONER_STATE_FALL3 = 15
	};

	void InitializePrisoner(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, PRISONER_STOP_ANIM);
	}

	void ControlPrisoner(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short head = 0;
		auto extraTorsoRot = EulerAngles::Identity;

		// Blocked box damage
		if (item->BoxNumber != NO_VALUE && (g_Level.PathfindingBoxes[item->BoxNumber].flags & BLOCKED))
		{
			DoDamage(item, 20);
			DoLotsOfBlood(item->Pose.Position.x, item->Pose.Position.y - (GetRandomControl() & 255) - 32,
				item->Pose.Position.z, (GetRandomControl() & 127) + 128, GetRandomControl() << 1, item->RoomNumber, 3);
		}

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != PRISONER_STATE_DEATH)
			{
				SetAnimation(*item, PRISONER_DIE_ANIM);
				creature->LOT.Step = CLICK(1);
			}
		}
		else
		{
			if (item->AIBits && item->AIBits != MODIFY)
			{
				GetAITarget(creature);
			}
			else if (creature->HurtByLara)
			{
				creature->Enemy = LaraItem;
			}
			else
			{
				// Find nearest enemy target (not Lara, not other Prisoners)
				TargetNearestEntity(*item, PrisonerExcludedTargets);
			}

			// Indestructible Prisoner with MODIFY flag. "HitPoints" property overrides the refill value.
			if (item->AIBits == MODIFY)
				item->HitPoints = PropertyHandler::Get(*item, PropName_HitPoints, 200);

			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			// Don't target Lara if not hurt by her
			if (!creature->HurtByLara && creature->Enemy == LaraItem)
				creature->Enemy = nullptr;

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

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			// Alert guards only when hurt by Lara
			if (creature->HurtByLara)
			{
				if (!creature->Alerted)
					SoundEffect(SFX_TR3_AMERCAN_HOY, &item->Pose);

				AlertAllGuards(itemNumber);
			}

			auto* enemy = creature->Enemy.Get();

			switch (item->Animation.ActiveState)
			{
			case PRISONER_STATE_WAIT:
			case PRISONER_STATE_STOP:
				// WAIT bails out early once alerted or already heading into a run.
				if (item->Animation.ActiveState == PRISONER_STATE_WAIT &&
					(creature->Alerted || item->Animation.TargetState == PRISONER_STATE_RUN))
				{
					item->Animation.TargetState = PRISONER_STATE_STOP;
					break;
				}

				creature->Flags = 0;
				creature->MaxTurn = 0;
				head = laraAI.angle;

				if (item->AIBits & GUARD)
				{
					head = AIGuard(creature);
					if (Random::TestProbability(1 / 256.0f))
					{
						if (item->Animation.ActiveState == PRISONER_STATE_STOP)
							item->Animation.TargetState = PRISONER_STATE_WAIT;
						else
							item->Animation.TargetState = PRISONER_STATE_STOP;
					}
				}
				else if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = PRISONER_STATE_WALK;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && ai.ahead && !item->HitStatus)
						item->Animation.TargetState = PRISONER_STATE_STOP;
					else
						item->Animation.TargetState = PRISONER_STATE_RUN;
				}
				else if (creature->Mood == MoodType::Bored ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else if (ai.ahead)
						item->Animation.TargetState = PRISONER_STATE_STOP;
					else
						item->Animation.TargetState = PRISONER_STATE_RUN;
				}
				else if (ai.bite && ai.distance < PRISONER_ATTACK0_RANGE)
				{
					item->Animation.TargetState = PRISONER_STATE_AIM0;
				}
				else if (ai.bite && ai.distance < PRISONER_ATTACK1_RANGE)
				{
					item->Animation.TargetState = PRISONER_STATE_AIM1;
				}
				else if (ai.bite && ai.distance < PRISONER_WALK_RANGE)
				{
					item->Animation.TargetState = PRISONER_STATE_WALK;
				}
				else
				{
					item->Animation.TargetState = PRISONER_STATE_RUN;
				}

				break;

			case PRISONER_STATE_WALK:
				head = laraAI.angle;
				creature->MaxTurn = PRISONER_WALK_TURN_RATE_MAX;

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = PRISONER_STATE_WALK;
					head = 0;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = PRISONER_STATE_RUN;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 256.0f))
					{
						item->Animation.RequiredState = PRISONER_STATE_WAIT;
						item->Animation.TargetState = PRISONER_STATE_STOP;
					}
				}
				else if (ai.bite && ai.distance < PRISONER_ATTACK0_RANGE)
				{
					item->Animation.TargetState = PRISONER_STATE_STOP;
				}
				else if (ai.bite && ai.distance < PRISONER_ATTACK2_RANGE)
				{
					item->Animation.TargetState = PRISONER_STATE_AIM2;
				}
				else
				{
					item->Animation.TargetState = PRISONER_STATE_RUN;
				}

				break;

			case PRISONER_STATE_RUN:
				if (ai.ahead)
					head = ai.angle;

				creature->MaxTurn = PRISONER_RUN_TURN_RATE_MAX;
				tilt = angle / 2;

				if (item->AIBits & GUARD)
				{
					item->Animation.TargetState = PRISONER_STATE_STOP;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && ai.ahead)
						item->Animation.TargetState = PRISONER_STATE_STOP;
				}
				else if ((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2))))
				{
					item->Animation.TargetState = PRISONER_STATE_STOP;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					item->Animation.TargetState = PRISONER_STATE_WALK;
				}
				else if (ai.ahead && ai.distance < PRISONER_WALK_RANGE)
				{
					item->Animation.TargetState = PRISONER_STATE_WALK;
				}

				break;

			case PRISONER_STATE_AIM0:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PRISONER_WALK_TURN_RATE_MAX;
				creature->Flags = 0;

				if (ai.bite && ai.distance < PRISONER_ATTACK0_RANGE)
					item->Animation.TargetState = PRISONER_STATE_PUNCH0;
				else
					item->Animation.TargetState = PRISONER_STATE_STOP;

				break;

			case PRISONER_STATE_AIM1:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PRISONER_WALK_TURN_RATE_MAX;
				creature->Flags = 0;

				if (ai.ahead && ai.distance < PRISONER_ATTACK1_RANGE)
					item->Animation.TargetState = PRISONER_STATE_PUNCH1;
				else
					item->Animation.TargetState = PRISONER_STATE_STOP;

				break;

			case PRISONER_STATE_AIM2:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PRISONER_WALK_TURN_RATE_MAX;
				creature->Flags = 0;

				if (ai.bite && ai.distance < PRISONER_ATTACK2_RANGE)
					item->Animation.TargetState = PRISONER_STATE_PUNCH2;
				else
					item->Animation.TargetState = PRISONER_STATE_WALK;

				break;

			case PRISONER_STATE_PUNCH0:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PRISONER_WALK_TURN_RATE_MAX;

				if (enemy == LaraItem)
				{
					if (!creature->Flags && item->TouchBits.Test(PrisonerAttackJoints))
					{
						DoDamage(creature->Enemy, PRISONER_HIT_DAMAGE);
						CreatureEffect(item, PrisonerHitBite, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 1;
					}
				}
				else if (!creature->Flags && enemy != nullptr)
				{
					float distance = Vector3i::Distance(item->Pose.Position, enemy->Pose.Position);
					if (distance < PRISONER_HIT_RADIUS)
					{
						DoDamage(enemy, PRISONER_HIT_DAMAGE / 2);
						CreatureEffect(item, PrisonerHitBite, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 1;
					}
				}

				break;

			case PRISONER_STATE_PUNCH1:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PRISONER_WALK_TURN_RATE_MAX;

				if (enemy == LaraItem)
				{
					if (!creature->Flags && item->TouchBits.Test(PrisonerAttackJoints))
					{
						DoDamage(creature->Enemy, PRISONER_HIT_DAMAGE);
						CreatureEffect(item, PrisonerHitBite, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 1;
					}
				}
				else if (!creature->Flags && enemy != nullptr)
				{
					float distance = Vector3i::Distance(item->Pose.Position, enemy->Pose.Position);
					if (distance < PRISONER_HIT_RADIUS)
					{
						DoDamage(enemy, PRISONER_HIT_DAMAGE / 2);
						CreatureEffect(item, PrisonerHitBite, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 1;
					}
				}

				if (ai.ahead && ai.distance > PRISONER_ATTACK1_RANGE && ai.distance < PRISONER_ATTACK2_RANGE)
					item->Animation.TargetState = PRISONER_STATE_PUNCH2;

				break;

			case PRISONER_STATE_PUNCH2:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PRISONER_WALK_TURN_RATE_MAX;

				if (enemy == LaraItem)
				{
					if (creature->Flags != 2 && item->TouchBits.Test(PrisonerAttackJoints))
					{
						DoDamage(creature->Enemy, PRISONER_SWIPE_DAMAGE);
						CreatureEffect(item, PrisonerHitBite, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 2;
					}
				}
				else if (creature->Flags != 2 && enemy != nullptr)
				{
					float distance = Vector3i::Distance(item->Pose.Position, enemy->Pose.Position);
					if (distance < PRISONER_HIT_RADIUS)
					{
						DoDamage(enemy, PRISONER_SWIPE_DAMAGE / 2);
						CreatureEffect(item, PrisonerHitBite, DoBloodSplat);
						SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);
						creature->Flags = 2;
					}
				}

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, head);

		if (item->Animation.ActiveState < PRISONER_STATE_DEATH)
		{
			switch (CreatureVault(itemNumber, angle, 2, PRISONER_VAULT_SHIFT))
			{
			case 2:
				creature->MaxTurn = 0;
				SetAnimation(*item, PRISONER_CLIMB1_ANIM);
				break;

			case 3:
				creature->MaxTurn = 0;
				SetAnimation(*item, PRISONER_CLIMB2_ANIM);
				break;

			case 4:
				creature->MaxTurn = 0;
				SetAnimation(*item, PRISONER_CLIMB3_ANIM);
				break;

			case -4:
				creature->MaxTurn = 0;
				SetAnimation(*item, PRISONER_FALL3_ANIM);
				break;
			}
		}
		else
		{
			creature->MaxTurn = 0;
			CreatureAnimation(itemNumber, angle, 0);
		}
	}
}
