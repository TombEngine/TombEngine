#include "framework.h"
#include "Objects/TR3/Entity/tr3_flamethrower.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto FLAMETHROWER_ATTACK_RANGE = SQUARE(BLOCK(1.5f));
	constexpr auto FLAMETHROWER_IDLE_RANGE	 = SQUARE(BLOCK(2));
	constexpr auto FLAMETHROWER_AWARE_RANGE	 = SQUARE(BLOCK(8));

	constexpr auto FLAMETHROWER_WALK_TURN_RATE_MAX = ANGLE(5.0f);

	const auto FlamethrowerBite = CreatureBiteInfo(Vector3(0, 340, 64), 7);
	const auto FlamethrowerTargetIds = { ID_LARA, ID_SEAL_MUTANT };

	// TODO
	enum FlamethrowerState
	{
		// No state 0.
		FLAMETHROWER_STATE_IDLE = 1,
		FLAMETHROWER_STATE_WALK_FORWARD = 2,
		FLAMETHROWER_STATE_RUN = 3,
		FLAMETHROWER_STATE_WAIT = 4,
		FLAMETHROWER_STATE_WALK_FORWARD_ATTACK = 6,
		FLAMETHROWER_STATE_DEATH = 7,
		FLAMETHROWER_STATE_WALK_FORWARD_AIM = 9,
		FLAMETHROWER_STATE_AIM_1 = 10,
		FLAMETHROWER_STATE_ATTACK_1 = 11
	};

	enum FlamethrowerAnim
	{
		FLAME_ANIM_IDLE = 12,
		FLAME_ANIM_DEATH = 19
	};

	// TODO: Remove LaraItem dependencies.
	void FlameThrowerControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		auto pos = GetJointPosition(item, FlamethrowerBite);
		int randomInt = GetRandomControl();

		if (item->Animation.ActiveState != 6 && item->Animation.ActiveState != 11)
		{
			SpawnDynamicLight(pos.x, pos.y, pos.z, (randomInt & 3) + 6, 24 - ((randomInt / 16) & 3), 16 - ((randomInt / 64) & 3), randomInt & 3);
			TriggerPilotFlame(itemNumber, 9);
		}
		else
		{
			SpawnDynamicLight(pos.x, pos.y, pos.z, (randomInt & 3) + 10, 31 - ((randomInt / 16) & 3), 24 - ((randomInt / 64) & 3), randomInt & 7);
		}

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != FLAMETHROWER_STATE_DEATH)
				SetAnimation(*item, FLAME_ANIM_DEATH);
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
				TargetNearestEntity(*item, FlamethrowerTargetIds, false);
			}

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			AI_INFO laraAI;
			if (creature->Enemy == LaraItem)
			{
				laraAI.angle = AI.angle;
				laraAI.distance = AI.distance;

				if (!creature->HurtByLara)
					creature->Enemy = nullptr;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;

				laraAI.angle = phd_atan(dz, dz) - item->Pose.Orientation.y;
				laraAI.distance = SQUARE(dz) + SQUARE(dx);

				AI.xAngle -= ANGLE(11.25f);
			}

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			angle = CreatureTurn(item, creature->MaxTurn);
			auto* realEnemy = creature->Enemy;

			bool canAttack = ((realEnemy != nullptr && !realEnemy->IsLara()) || creature->HurtByLara);

			if (item->HitStatus || laraAI.distance < FLAMETHROWER_AWARE_RANGE || TargetVisible(item, &laraAI))
			{
				if (!creature->Alerted)
					SoundEffect(SFX_TR3_AMERCAN_HOY, &item->Pose);

				AlertAllGuards(itemNumber);
			}

			switch (item->Animation.ActiveState)
			{
			case FLAMETHROWER_STATE_IDLE:
				creature->MaxTurn = 0;
				creature->Flags = 0;
				extraHeadRot.y = laraAI.angle;

				if (item->AIBits & GUARD)
				{
					extraHeadRot.y = AIGuard(creature);

					if (Random::TestProbability(1.0f / 128))
						item->Animation.TargetState = FLAMETHROWER_STATE_WAIT;

					break;
				}
				else if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;
				}
				else if (Targetable(item, &AI) && canAttack)
				{
					if (AI.distance < FLAMETHROWER_ATTACK_RANGE)
						item->Animation.TargetState = FLAMETHROWER_STATE_AIM_1;
					else
						item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;
				}
				else if (creature->Mood == MoodType::Bored && AI.ahead && Random::TestProbability(1 / 128.0f))
				{
					item->Animation.TargetState = FLAMETHROWER_STATE_WAIT;
				}
				else if (creature->Mood == MoodType::Attack || Random::TestProbability(1 / 128.0f))
				{
					item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;
				}

				break;

			case 4:
				extraHeadRot.y = laraAI.angle;

				if (item->AIBits & GUARD)
				{
					extraHeadRot.y = AIGuard(creature);

					if (Random::TestProbability(1 / 128.0f))
						item->Animation.TargetState = FLAMETHROWER_STATE_IDLE;

					break;
				}
				else if ((Targetable(item, &AI) &&
					AI.distance < FLAMETHROWER_ATTACK_RANGE && canAttack ||
					creature->Mood != MoodType::Bored ||
					Random::TestProbability(1 / 128.0f)))
				{
					item->Animation.TargetState = FLAMETHROWER_STATE_IDLE;
				}

				break;

			case FLAMETHROWER_STATE_WALK_FORWARD:
				creature->MaxTurn = FLAMETHROWER_WALK_TURN_RATE_MAX;
				creature->Flags = 0;
				extraHeadRot.y = laraAI.angle;

				if (item->AIBits & GUARD)
					SetAnimation(*item, FLAME_ANIM_IDLE);
				else if (item->AIBits & PATROL1)
					item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;
				else if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;
				else if (Targetable(item, &AI) && canAttack)
				{
					if (AI.distance < FLAMETHROWER_IDLE_RANGE)
						item->Animation.TargetState = FLAMETHROWER_STATE_IDLE;
					else if (AI.distance < FLAMETHROWER_ATTACK_RANGE)
						item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD_AIM;
				}
				else if (creature->Mood == MoodType::Bored && AI.ahead)
					item->Animation.TargetState = FLAMETHROWER_STATE_IDLE;
				else
					item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;

				break;

			case FLAMETHROWER_STATE_AIM_1:
				creature->Flags = 0;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;

					if (Targetable(item, &AI) &&
						AI.distance < FLAMETHROWER_ATTACK_RANGE && canAttack)
					{
						item->Animation.TargetState = FLAMETHROWER_STATE_ATTACK_1;
					}
					else
						item->Animation.TargetState = FLAMETHROWER_STATE_IDLE;
				}

				break;

			case FLAMETHROWER_STATE_WALK_FORWARD_AIM:
				creature->Flags = 0;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;

					if (Targetable(item, &AI) &&
						AI.distance < FLAMETHROWER_ATTACK_RANGE && canAttack)
					{
						item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD_ATTACK;
					}
					else
						item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;
				}

				break;

			case FLAMETHROWER_STATE_ATTACK_1:
				if (creature->Flags < 40)
					creature->Flags += (creature->Flags / 4) + 1;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;

					if (Targetable(item, &AI) &&
						AI.distance < FLAMETHROWER_ATTACK_RANGE && canAttack)
					{
						item->Animation.TargetState = FLAMETHROWER_STATE_ATTACK_1;
					}
					else
						item->Animation.TargetState = FLAMETHROWER_STATE_IDLE;
				}
				else
					item->Animation.TargetState = FLAMETHROWER_STATE_IDLE;

				if (creature->Flags < 40)
					ThrowFire(itemNumber, FlamethrowerBite, Vector3i(0, creature->Flags * 1.5f, 0));
				else
				{
					ThrowFire(itemNumber, FlamethrowerBite, Vector3i(0, (Random::GenerateInt() & 63) + 12, 0));
					if (realEnemy && realEnemy->ObjectNumber == ID_SEAL_MUTANT)
						realEnemy->ItemFlags[0]++;
				}

				SoundEffect(SFX_TR4_FLAME_EMITTER, &item->Pose);
				break;

			case FLAMETHROWER_STATE_WALK_FORWARD_ATTACK:
				if (creature->Flags < 40)
					creature->Flags += (creature->Flags / 4) + 1;

				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;

					if (Targetable(item, &AI) &&
						AI.distance < FLAMETHROWER_ATTACK_RANGE && canAttack)
					{
						item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD_ATTACK;
					}
					else
						item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;
				}
				else
					item->Animation.TargetState = FLAMETHROWER_STATE_WALK_FORWARD;

				if (creature->Flags < 40)
					ThrowFire(itemNumber, FlamethrowerBite, Vector3i(0, creature->Flags * 1.5f, 0));
				else
				{
					ThrowFire(itemNumber, FlamethrowerBite, Vector3i(0, (GetRandomControl() & 63) + 12, 0));
					if (realEnemy && realEnemy->ObjectNumber == ID_SEAL_MUTANT)
						realEnemy->ItemFlags[0]++;
				}

				SoundEffect(SFX_TR4_FLAME_EMITTER, &item->Pose);
				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureAnimation(itemNumber, angle, 0);
	}
}
