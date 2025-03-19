#include "framework.h"
#include "Objects/TR3/Entity/SealMutant.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Effects/enemy_missile.h"

using namespace TEN::Math;

// NOTES:
// ItemFlags[0]: Sprite ID for poison effect.

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto SEAL_MUTANT_ATTACK_DAMAGE = 1;

	constexpr auto SEAL_MUTANT_ALERT_RANGE	= SQUARE(BLOCK(1));
	constexpr auto SEAL_MUTANT_ATTACK_RANGE = SQUARE(BLOCK(1.5f));

	constexpr auto SEAL_MUTANT_WALK_TURN_RATE = ANGLE(3.0f);

	constexpr auto SEAL_MUTANT_FLAME_LIGHT_Y_OFFSET = CLICK(2);
	constexpr auto SEAL_MUTANT_BURN_END_TIME		= 16;

	const auto SealMutantGasBite			   = CreatureBiteInfo(Vector3(0.0f, 48.0f, 140.0f), 10);
	const auto SealMutantAttackTargetObjectIds = { ID_LARA, ID_FLAMETHROWER_BADDY, ID_WORKER_FLAMETHROWER };

	enum SealMutantState
	{
		SEAL_MUTANT_STATE_IDLE = 0,
		SEAL_MUTANT_STATE_WALK = 1,
		SEAL_MUTANT_STATE_ATTACK = 2,
		SEAL_MUTANT_STATE_DEATH = 3,
		SEAL_MUTANT_STATE_TRAP = 4
	};

	enum SealMutantAnim
	{
		SEAL_MUTANT_ANIM_IDLE = 0,
		SEAL_MUTANT_ANIM_IDLE_TO_WALK = 1,
		SEAL_MUTANT_ANIM_WALK = 2,
		SEAL_MUTANT_ANIM_WALK_TO_IDLE = 3,
		SEAL_MUTANT_ANIM_ATTACK = 4,
		SEAL_MUTANT_ANIM_DEATH = 5,
		SEAL_MUTANT_ANIM_TRAP = 6
	};

	enum SealMutantItemFlags
	{
		IF_SEAL_MUTANT_FLAME_TIMER = 0
	};

	enum SealMutantOcb
	{
		OCB_NORMAL_BEHAVIOUR = 0,
		OCB_TRAP = 1
	};

	static void SpawnSealMutantPoisonGas(ItemInfo& item, float vel)
	{
		constexpr auto GAS_COUNT			 = 2;
		constexpr auto VEL_MULT				 = 5.0f;
		constexpr auto PLAYER_CROUCH_GRAVITY = 32.0f;

		auto& creature = *GetCreatureInfo(&item);

		// HACK.
		float gravity = 0.0f;
		if (creature.Enemy != nullptr)
		{
			if (creature.Enemy->IsLara())
			{
				const auto& player = GetLaraInfo(*creature.Enemy);
				if (player.Control.IsLow)
					gravity = PLAYER_CROUCH_GRAVITY;
			}
			else
			{
				DoDamage(creature.Enemy, SEAL_MUTANT_ATTACK_DAMAGE);
			}
		}
		
		auto velVector = Vector3(0.0f, gravity, vel * VEL_MULT);
		auto colorStart  = Color(Random::GenerateFloat(0.25f, 0.5f), Random::GenerateFloat(0.25f, 0.5f), 0.1f);
		auto colorEnd = Color(Random::GenerateFloat(0.05f, 0.1f), Random::GenerateFloat(0.05f, 0.1f), 0.0f);

		for (int i = 0; i < GAS_COUNT; i++)
			ThrowPoison(item, SealMutantGasBite, velVector, colorStart, colorEnd, item.ItemFlags[0]);
	}

	void InitializeSealMutant(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.ItemFlags[0] = 0;
		InitializeCreature(itemNumber);
	}

	void ControlSealMutant(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;
		auto headOrient = EulerAngles::Identity;
		auto torsoOrient = EulerAngles::Identity;

		float gasVel = 0.0f;

		if (item.TestOcb(OCB_TRAP))
		{
			if (item.Animation.ActiveState != SEAL_MUTANT_STATE_TRAP)
			{
				SetAnimation(item, SEAL_MUTANT_ANIM_TRAP);
			}
			else if (TestAnimFrameRange(item, 1, 124))
			{
				const auto& anim = GetAnimData(item);

				gasVel = item.Animation.FrameNumber - 1;
				if (gasVel > 24.0f)
				{
					gasVel = item.Animation.FrameNumber - (anim.EndFrameNumber - 8);
					if (gasVel <= 0.0f)
						gasVel = 1.0f;

					if (gasVel > 24.0f)
						gasVel = Random::GenerateFloat(8.0f, 24.0f);

					SpawnSealMutantPoisonGas(item, gasVel);
				}
			}

			CreatureAnimation(itemNumber, 0, 0);
			return;
		}

		if (item.GetFlagField(IF_SEAL_MUTANT_FLAME_TIMER) > 80)
			item.HitPoints = 0;

		if (item.HitPoints <= 0)
		{
			const auto& anim = GetAnimData(item);

			if (item.Animation.ActiveState != SEAL_MUTANT_STATE_DEATH)
			{
				SetAnimation(item, SEAL_MUTANT_ANIM_DEATH);
			}
			else if (item.GetFlagField(IF_SEAL_MUTANT_FLAME_TIMER) > 80)
			{
				for (int boneID = 9; boneID < 17; boneID++)
				{
					auto pos = GetJointPosition(item, boneID);
					TriggerFireFlame(pos.x, pos.y, pos.z, FlameType::Medium);
				}

				int burnTimer = item.Animation.FrameNumber;
				if (burnTimer > SEAL_MUTANT_BURN_END_TIME)
				{
					burnTimer = item.Animation.FrameNumber - anim.EndFrameNumber;
					if (burnTimer > SEAL_MUTANT_BURN_END_TIME)
						burnTimer = SEAL_MUTANT_BURN_END_TIME;
				}

				if (burnTimer != SEAL_MUTANT_BURN_END_TIME)
				{
					auto pos = GetJointPosition(item, SealMutantGasBite.BoneID, Vector3(0.0f, -SEAL_MUTANT_FLAME_LIGHT_Y_OFFSET, 0.0f));
					auto color = Color(Random::GenerateFloat(0.75f, 1.0f), Random::GenerateFloat(0.4f, 0.5f), Random::GenerateFloat(0.0f, 0.25f));
					float falloff = Random::GenerateFloat(BLOCK(1.5f), BLOCK(2.5f));
					SpawnDynamicPointLight(pos.ToVector3(), color, falloff);
				}
			}
			else if (TestAnimFrameRange(item, 1, 124))
			{
				gasVel = item.Animation.FrameNumber - 1;
				if (gasVel > 24.0f)
				{
					gasVel = item.Animation.FrameNumber - (anim.EndFrameNumber - 8);
					if (gasVel <= 0.0f)
						gasVel = 1.0f;

					if (gasVel > 24.0f)
						gasVel = Random::GenerateFloat(16.0f, 24.0f);

					SpawnSealMutantPoisonGas(item, gasVel);
				}
			}
		}
		else
		{
			if (item.AIBits)
			{
				GetAITarget(&creature);
			}
			else
			{
				TargetNearestEntity(item, SealMutantAttackTargetObjectIds, false);
			}

			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			GetCreatureMood(&item, &ai, ai.zoneNumber == ai.enemyZone);
			if (creature.Enemy != nullptr && creature.Enemy->IsLara())
			{
				const auto& player = GetLaraInfo(*creature.Enemy);
				if (player.Status.Poison >= LARA_POISON_MAX)
					creature.Mood = MoodType::Escape;
			}

			CreatureMood(&item, &ai, ai.zoneNumber == ai.enemyZone);
			headingAngle = CreatureTurn(&item, creature.MaxTurn);
			
			auto* target = creature.Enemy;
			creature.Enemy = LaraItem;
			if (ai.distance < SEAL_MUTANT_ALERT_RANGE || item.HitStatus || TargetVisible(&item, &ai))
				AlertAllGuards(itemNumber);

			creature.Enemy = target;

			switch (item.Animation.ActiveState)
			{
			case SEAL_MUTANT_STATE_IDLE:
				creature.MaxTurn = 0;
				creature.Flags = 0;
				headOrient.x = -ai.xAngle;
				headOrient.y = ai.angle;
				torsoOrient.x = 0;
				torsoOrient.z = 0;

				if (item.AIBits & GUARD)
				{
					item.Animation.TargetState = SEAL_MUTANT_STATE_IDLE;
					headOrient.x = 0;
					headOrient.y = AIGuard(&creature);
				}
				else if (item.AIBits & PATROL1)
				{
					item.Animation.TargetState = SEAL_MUTANT_STATE_WALK;
					headOrient.x = 0;
					headOrient.y = 0;
				}
				else if (creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = SEAL_MUTANT_STATE_WALK;
				}
				else if (Targetable(&item, &ai) && ai.distance < SEAL_MUTANT_ATTACK_RANGE)
				{
					item.Animation.TargetState = SEAL_MUTANT_STATE_ATTACK;
				}
				else if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else
				{
					item.Animation.TargetState = SEAL_MUTANT_STATE_WALK;
				}

				break;

			case SEAL_MUTANT_STATE_WALK:
				creature.MaxTurn = SEAL_MUTANT_WALK_TURN_RATE;

				if (ai.ahead)
				{
					headOrient.x = -ai.xAngle;
					headOrient.y = ai.angle;
				}

				if (item.AIBits & PATROL1)
				{
					item.Animation.TargetState = SEAL_MUTANT_STATE_WALK;
					headOrient.y = 0;
				}
				else if (Targetable(&item, &ai) && ai.distance < SEAL_MUTANT_ATTACK_RANGE)
				{
					item.Animation.TargetState = SEAL_MUTANT_STATE_IDLE;
				}

				break;

			case SEAL_MUTANT_STATE_ATTACK:
				if (ai.ahead)
				{
					headOrient.x = -ai.xAngle;
					headOrient.y = ai.angle;
					torsoOrient.x = -ai.xAngle / 2;
					torsoOrient.z = ai.angle / 2;
				}

				if (TestAnimFrameRange(item, 35, 58))
				{
					if (creature.Flags < 24)
						creature.Flags += 3;

					gasVel = 0.0f;
					if (creature.Flags < 24.0f)
					{
						gasVel = creature.Flags;
					}
					else
					{
						gasVel = Random::GenerateFloat(16.0f, 24.0f);
					}

					SpawnSealMutantPoisonGas(item, gasVel);
					if (creature.Enemy != nullptr && !creature.Enemy->IsLara())
						creature.Enemy->HitStatus = true;
				}

				break;
			}
		}

		CreatureJoint(&item, 0, torsoOrient.z);
		CreatureJoint(&item, 1, torsoOrient.x);
		CreatureJoint(&item, 2, headOrient.y);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
