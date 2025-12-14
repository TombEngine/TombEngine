#include "framework.h"
#include "Objects/TR2/Entity/tr2_spear_guardian.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR2
{
	// TODO: Fix names.
	constexpr auto SPEAR_GUARDIAN_BASIC_DAMAGE	  = 75;
	constexpr auto SPEAR_GUARDIAN_POWERFUL_DAMAGE = 120;

	constexpr auto SPEAR_GUARDIAN_WALK_TURN_RATE_MAX = ANGLE(3.0f);
	constexpr auto SPEAR_GUARDIAN_RUN_TURN_RATE_MAX	 = ANGLE(5.0f);

	// TODO: Fix names.
	constexpr auto SPEAR_GUARDIAN_SLASH_RANGE			   = SQUARE(BLOCK(1));
	constexpr auto SPEAR_GUARDIAN_DOUBLE_ATTACK_RANGE	   = SQUARE(BLOCK(1));
	constexpr auto SPEAR_GUARDIAN_DOUBLE_ATTACK_WALK_RANGE = SQUARE(BLOCK(1.5f));
	constexpr auto SPEAR_GUARDIAN_RUN_RANGE				   = SQUARE(BLOCK(3));
	constexpr auto SPEAR_GUARDIAN_ATTACK_RANGE			   = SQUARE(BLOCK(2));

	constexpr auto SPEAR_GUARDIAN_SWAPMESH_TIME = 3;

	const auto SpearGuardianBiteLeft  = CreatureBiteInfo(Vector3(0, 0, 920), 11);
	const auto SpearGuardianBiteRight = CreatureBiteInfo(Vector3(0, 0, 920), 18);

	enum SpearGuardianState
	{
		// No state 0.
		SPEAR_GUARDIAN_STATE_IDLE = 1,
		SPEAR_GUARDIAN_STATE_SLASH_IDLE = 2,
		SPEAR_GUARDIAN_STATE_WALK = 3,
		SPEAR_GUARDIAN_STATE_RUN = 4,
		SPEAR_GUARDIAN_STATE_DOUBLE_ATTACK_FRONT_AIM = 5,
		SPEAR_GUARDIAN_STATE_DOUBLE_ATTACK_FRONT = 6,
		SPEAR_GUARDIAN_STATE_WALK_DOUBLE_ATTACK_FRONT_AIM = 7,
		SPEAR_GUARDIAN_STATE_WALK_DOUBLE_ATTACK_FRONT = 8,
		SPEAR_GUARDIAN_STATE_WALK_ATTACK_LEFT_SPEAR_AIM = 9,
		SPEAR_GUARDIAN_STATE_WALK_ATTACK_LEFT_SPEAR = 10,
		SPEAR_GUARDIAN_STATE_WALK_ATTACK_RIGHT_SPEAR_AIM = 11,
		SPEAR_GUARDIAN_STATE_WALK_ATTACK_RIGHT_SPEAR = 12,
		SPEAR_GUARDIAN_STATE_SLASH_AIM = 13,
		SPEAR_GUARDIAN_STATE_SLASH = 14,
		SPEAR_GUARDIAN_STATE_RUN_DOUBLE_ATTACK_FRONT_AIM = 15,
		SPEAR_GUARDIAN_STATE_RUN_DOUBLE_ATTACK_FRONT = 16,
		SPEAR_GUARDIAN_STATE_DEATH = 17,
		SPEAR_GUARDIAN_STATE_AWAKE = 18,
		SPEAR_GUARDIAN_STATE_KILL = 19
	};

	// TODO: Fix names.
	enum SpearGuardianAnim
	{
		SPEAR_GUARDIAN_ANIM_DOUBLE_ATTACK_FRONT_CANCEL = 0,
		SPEAR_GUARDIAN_ANIM_DOUBLE_ATTACK_FRONT_PREPARE = 1,
		SPEAR_GUARDIAN_ANIM_WALK_DOUBLE_ATTACK_FRONT_CANCEL = 2,
		SPEAR_GUARDIAN_ANIM_WALK_DOUBLE_ATTACK_FRONT_PREPARE = 3,
		SPEAR_GUARDIAN_ANIM_WALK_LEFT_SPEAR_ATTACK_CANCEL = 4,
		SPEAR_GUARDIAN_ANIM_WALK_LEFT_SPEAR_ATTACK_PREPARE = 5,
		SPEAR_GUARDIAN_ANIM_WALK_RIGHT_SPEAR_ATTACK_CANCEL = 6,
		SPEAR_GUARDIAN_ANIM_WALK_RIGHT_SPEAR_ATTACK_PREPARE = 7,
		SPEAR_GUARDIAN_ANIM_SLASH_PREPARE = 8,
		SPEAR_GUARDIAN_ANIM_SLASH_CANCEL = 9,
		SPEAR_GUARDIAN_ANIM_WALK_DOUBLE_ATTACK_FRONT_TO_IDLE_FAST = 10,
		SPEAR_GUARDIAN_ANIM_WALK_DOUBLE_ATTACK_FRONT = 11, // Deal damage.
		SPEAR_GUARDIAN_ANIM_UNKNOWN = 12,

		SPEAR_GUARDIAN_ANIM_WALK_DOUBLE_ATTACK_FRONT_TO_SLASH_PREPARE = 15,

		SPEAR_GUARDIAN_ANIM_SLASH = 24,

		SPEAR_GUARDIAN_ANIM_RUN_TO_WALK = 27,
		SPEAR_GUARDIAN_ANIM_RUN = 28,
		SPEAR_GUARDIAN_ANIM_STAND_TO_SLASH_PREPARE = 29,
		SPEAR_GUARDIAN_ANIM_STAND_TO_WALK = 30,
		SPEAR_GUARDIAN_ANIM_SLASH_PREPARE_TO_STAND = 31,
		SPEAR_GUARDIAN_ANIM_SLASH_PREPARE_TO_WALK = 32,
		SPEAR_GUARDIAN_ANIM_IDLE = 33,
		SPEAR_GUARDIAN_ANIM_SLASH_IDLE = 34,
		SPEAR_GUARDIAN_ANIM_WALK = 35,
		SPEAR_GUARDIAN_ANIM_WALK_TO_RUN = 36,
		SPEAR_GUARDIAN_ANIM_RUN_TO_STAND = 37,
		SPEAR_GUARDIAN_ANIM_WALK_TO_STAND = 38,
		SPEAR_GUARDIAN_ANIM_RUN_TO_IDLE = 39,
		SPEAR_GUARDIAN_ANIM_RUN_TO_SLASH_IDLE = 40,
		SPEAR_GUARDIAN_ANIM_RUN_TO_DOUBLE_ATTACK_FRONT = 41,
		SPEAR_GUARDIAN_ANIM_RUN_DOUBLE_ATTACK_FRONT = 42,

		SPEAR_GUARDIAN_ANIM_WALK_TO_SLASH_IDLE = 47,
		SPEAR_GUARDIAN_ANIM_AWAKE = 48,
		SPEAR_GUARDIAN_ANIM_KILL = 49,
	};

	static void DoSpearGuardianAttack(ItemInfo& item, int damage)
	{
		auto& creature = *GetCreatureInfo(&item);

		if (!(creature.Flags & 1) && item.TouchBits.Test(SpearGuardianBiteRight.BoneID))
		{
			DoDamage(creature.Enemy, damage);
			CreatureEffect(&item, SpearGuardianBiteRight, DoBloodSplat);
			creature.Flags |= 1;
			SoundEffect(SFX_TR2_CRUNCH2, &item.Pose);
		}

		if (!(creature.Flags & 2) && item.TouchBits.Test(SpearGuardianBiteLeft.BoneID))
		{
			DoDamage(creature.Enemy, damage);
			CreatureEffect(&item, SpearGuardianBiteLeft, DoBloodSplat);
			creature.Flags |= 2;
			SoundEffect(SFX_TR2_CRUNCH2, &item.Pose);
		}
	}

	static void MeshSwapNormalToJade(ItemInfo& item, int jointIndex, bool useEffect = true)
	{
		if (useEffect)
			SpawnSpearGuardianEffect(GetJointPosition(&item, jointIndex).ToVector3(), item.RoomNumber);

		item.Model.MeshIndex[jointIndex] = Objects[ID_SPEAR_GUARDIAN_STATUE].meshIndex + jointIndex;
	}

	static void MeshSwapJadeToNormal(ItemInfo& item, int jointIndex, bool useEffect = true)
	{
		if (useEffect)
			SpawnSpearGuardianEffect(GetJointPosition(&item, jointIndex).ToVector3(), item.RoomNumber);

		item.Model.MeshIndex[jointIndex] = Objects[ID_SPEAR_GUARDIAN].meshIndex + jointIndex;
	}

	static bool DoSpearGuardianMeshSwap(ItemInfo& item)
	{
		const auto& object = Objects[item.ObjectNumber];
		auto& creature = *GetCreatureInfo(&item);

		// Do mesh swaps.
		if (creature.Flags == 0)
		{
			// Jade to normal.
			if (item.ItemFlags[3] == 0)
			{
				MeshSwapJadeToNormal(item, item.ItemFlags[0]);
				item.ItemFlags[0]++;

				if (item.ItemFlags[0] >= object.nmeshes)
				{
					item.ItemFlags[0] = 0;
					item.ItemFlags[1] = 0;
					item.ItemFlags[3] = 0;
					return true;
				}
			}
			// Normal to jade.
			else
			{
				MeshSwapNormalToJade(item, item.ItemFlags[0]);
				item.ItemFlags[0]--;

				if (item.ItemFlags[0] < 0)
				{
					item.ItemFlags[0] = 0;
					item.ItemFlags[1] = 1;
					item.ItemFlags[3] = 1;
					return true;
				}
			}

			creature.Flags = SPEAR_GUARDIAN_SWAPMESH_TIME;
		}
		else
		{
			creature.Flags--;
		}

		return false;
	}

	void InitializeSpearGuardian(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, SPEAR_GUARDIAN_ANIM_AWAKE);
		item.Status = ITEM_NOT_ACTIVE;

		item.ItemFlags[0] = 0; // Joint index for mesh swap.
		item.ItemFlags[1] = 1; // Immune state (bool).
		item.ItemFlags[2] = 1; // 1 = swap to jade. 2 = swap to normal.
		item.ItemFlags[3] = 0; // Set to true if mesh is swapped to jade, otherwise false.

		const auto& object = Objects[item.ObjectNumber];
		for (int jointIndex = 0; jointIndex < object.nmeshes; jointIndex++)
			MeshSwapNormalToJade(item, jointIndex, false);
	}

	void SpearGuardianControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* object = &Objects[item->ObjectNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		bool isPlayerAlive = ((creature->Enemy != nullptr) && creature->Enemy->IsLara() && (creature->Enemy->HitPoints > 0));

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != SPEAR_GUARDIAN_STATE_DEATH)
			{
				item->ItemFlags[3] = 1;
				item->ItemFlags[0] = object->nmeshes - 1;
				item->Animation.ActiveState = SPEAR_GUARDIAN_STATE_DEATH;
			}

			if (DoSpearGuardianMeshSwap(*item))
				CreatureDie(itemNumber, true);

			return;
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			if (ai.ahead && item->Animation.ActiveState != SPEAR_GUARDIAN_STATE_AWAKE)
			{
				extraHeadRot.x = ai.xAngle / 2;
				extraHeadRot.y = ai.angle / 2;
			}

			switch (item->Animation.ActiveState)
			{
			case SPEAR_GUARDIAN_STATE_AWAKE:
				creature->MaxTurn = 0;

				DoSpearGuardianMeshSwap(*item);
				break;

			case SPEAR_GUARDIAN_STATE_IDLE:
				creature->MaxTurn = 0;
				item->ItemFlags[1] = 0; // Remove immunity.

				if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 64.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_SLASH_IDLE;
					else if (Random::TestProbability(1 / 30.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;
				}
				else if (ai.ahead && ai.distance < SPEAR_GUARDIAN_DOUBLE_ATTACK_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_DOUBLE_ATTACK_FRONT_AIM;
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;

				break;

			case SPEAR_GUARDIAN_STATE_SLASH_IDLE:
				creature->MaxTurn = 0;

				if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 64.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_IDLE;
					else if (Random::TestProbability(1 / 30.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;
				}
				else if (ai.ahead && ai.distance < SPEAR_GUARDIAN_SLASH_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_SLASH_AIM;
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;

				break;

			case SPEAR_GUARDIAN_STATE_WALK:
				creature->MaxTurn = SPEAR_GUARDIAN_WALK_TURN_RATE_MAX;

				if (creature->Mood == MoodType::Escape)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_RUN;
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 64.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_IDLE;
					else if (Random::TestProbability(1 / 30.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_SLASH_IDLE;
				}
				else if (ai.ahead && ai.distance < SPEAR_GUARDIAN_ATTACK_RANGE)
				{
					if (ai.distance < SPEAR_GUARDIAN_DOUBLE_ATTACK_WALK_RANGE)
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK_DOUBLE_ATTACK_FRONT_AIM;
					else if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK_ATTACK_LEFT_SPEAR_AIM;
					else
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK_ATTACK_RIGHT_SPEAR_AIM;
				}
				else if (!ai.ahead || ai.distance > SPEAR_GUARDIAN_RUN_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_RUN;

				break;

			case SPEAR_GUARDIAN_STATE_RUN:
				creature->MaxTurn = SPEAR_GUARDIAN_RUN_TURN_RATE_MAX;

				if (creature->Mood == MoodType::Escape)
					break;
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_IDLE;
					else
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_SLASH_IDLE;
				}
				else if (ai.ahead && ai.distance < SPEAR_GUARDIAN_RUN_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_RUN_DOUBLE_ATTACK_FRONT_AIM;

				break;

			case SPEAR_GUARDIAN_STATE_DOUBLE_ATTACK_FRONT_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!ai.ahead || ai.distance > SPEAR_GUARDIAN_DOUBLE_ATTACK_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_IDLE;
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_DOUBLE_ATTACK_FRONT;

				break;

			case SPEAR_GUARDIAN_STATE_WALK_DOUBLE_ATTACK_FRONT_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!ai.ahead || ai.distance > SPEAR_GUARDIAN_DOUBLE_ATTACK_WALK_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK_DOUBLE_ATTACK_FRONT;

				break;

			case SPEAR_GUARDIAN_STATE_WALK_ATTACK_LEFT_SPEAR_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!ai.ahead || ai.distance > SPEAR_GUARDIAN_RUN_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK_ATTACK_LEFT_SPEAR;

				break;

			case SPEAR_GUARDIAN_STATE_WALK_ATTACK_RIGHT_SPEAR_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!ai.ahead || ai.distance > SPEAR_GUARDIAN_RUN_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK_ATTACK_RIGHT_SPEAR;

				break;

			case SPEAR_GUARDIAN_STATE_SLASH_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!ai.ahead || ai.distance > SPEAR_GUARDIAN_SLASH_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_SLASH_IDLE;
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_SLASH;

				break;

			case SPEAR_GUARDIAN_STATE_RUN_DOUBLE_ATTACK_FRONT_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!ai.ahead || ai.distance > SPEAR_GUARDIAN_RUN_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_RUN;
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_RUN_DOUBLE_ATTACK_FRONT;

				break;

			case SPEAR_GUARDIAN_STATE_DOUBLE_ATTACK_FRONT:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				DoSpearGuardianAttack(*item, SPEAR_GUARDIAN_POWERFUL_DAMAGE);
				break;

			case SPEAR_GUARDIAN_STATE_WALK_DOUBLE_ATTACK_FRONT:
			case SPEAR_GUARDIAN_STATE_WALK_ATTACK_LEFT_SPEAR:
			case SPEAR_GUARDIAN_STATE_WALK_ATTACK_RIGHT_SPEAR:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				DoSpearGuardianAttack(
					*item,
					item->Animation.ActiveState == SPEAR_GUARDIAN_STATE_WALK_DOUBLE_ATTACK_FRONT ?
					SPEAR_GUARDIAN_POWERFUL_DAMAGE : SPEAR_GUARDIAN_BASIC_DAMAGE);

				if (ai.ahead && ai.distance < SPEAR_GUARDIAN_SLASH_RANGE)
				{
					if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_IDLE;
					else
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_SLASH_IDLE;
				}
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;

				break;

			case SPEAR_GUARDIAN_STATE_SLASH:
				DoSpearGuardianAttack(*item, SPEAR_GUARDIAN_BASIC_DAMAGE);

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (ai.ahead && ai.distance < SPEAR_GUARDIAN_SLASH_RANGE)
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_IDLE;
				else
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_SLASH_IDLE;

				break;

			case SPEAR_GUARDIAN_STATE_RUN_DOUBLE_ATTACK_FRONT:
				DoSpearGuardianAttack(*item, SPEAR_GUARDIAN_POWERFUL_DAMAGE);

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (ai.ahead && ai.distance < SPEAR_GUARDIAN_SLASH_RANGE)
				{
					if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_IDLE;
					else
						item->Animation.TargetState = SPEAR_GUARDIAN_STATE_SLASH_IDLE;
				}
				else if (ai.ahead && ai.distance < SPEAR_GUARDIAN_RUN_RANGE)
				{
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_WALK;
				}
				else
				{
					item->Animation.TargetState = SPEAR_GUARDIAN_STATE_RUN;
				}

				break;

			case SPEAR_GUARDIAN_STATE_KILL:
				creature->MaxTurn = 0;
				break;
			}
		}

		if (isPlayerAlive && creature->Enemy->HitPoints <= 0)
		{
			creature->MaxTurn = 0;
			CreatureKill(item, SPEAR_GUARDIAN_ANIM_KILL, LEA_SPEAR_GUARDIAN_DEATH, SPEAR_GUARDIAN_STATE_KILL, LS_DEATH);
			return;
		}

		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureJoint(item, 3, extraHeadRot.x);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}

	void SpearGuardianHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		if (target.ItemFlags[1] == 1)
		{
			if (pos.has_value())
				TriggerRicochetSpark(pos.value(), source.Pose.Orientation.y);

			return;
		}

		DefaultItemHit(target, source, pos, damage, isExplosive, jointIndex);
	}

	void SpawnSpearGuardianEffect(const Vector3& pos, int roomNumber)
	{
		auto& particle = *GetFreeParticle();

		bool isUnderwater = TestEnvironment(ENV_FLAG_WATER, pos, roomNumber);
		auto sphere = BoundingSphere(pos, 16);
		auto effectPos = Random::GeneratePointInSphere(sphere);

		particle.on = true;
		particle.blendMode = BlendMode::Additive;

		particle.x = effectPos.x;
		particle.y = effectPos.y;
		particle.z = effectPos.z;
		particle.xVel = Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f));
		particle.yVel = Random::GenerateInt(-BLOCK(1 / 8.0f), BLOCK(1 / 8.0f));
		particle.zVel = Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f));

		if (isUnderwater)
		{
			particle.sR = 0;
			particle.sG = 255;
			particle.sB = 0;
			particle.dR = 0;
			particle.dG = 0;
			particle.dB = 0;
		}
		else
		{
			particle.sR = 0;
			particle.sG = 0;
			particle.sB = 0;
			particle.dR = 0;
			particle.dG = 255;
			particle.dB = 0;
		}

		particle.colFadeSpeed = 8;
		particle.fadeToBlack = 64;
		particle.sLife = particle.life = Random::GenerateInt(72, 128);
		particle.extras = 0;
		particle.dynamic = -1;

		if (isUnderwater)
		{
			particle.yVel /= 16;
			particle.y += 32;
			particle.friction = 4 | 16;
		}
		else
		{
			particle.friction = 6;
		}

		particle.rotAng = Random::GenerateAngle();
		particle.rotAdd = Random::GenerateAngle(ANGLE(-0.2f), ANGLE(0.2f));
		particle.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
		particle.scalar = 3;

		if (isUnderwater)
		{
			particle.gravity = 0;
			particle.maxYvel = 0;
		}
		else
		{
			particle.gravity = Random::GenerateInt(-8, -4);
			particle.maxYvel = Random::GenerateInt(-8, -4);
		}

		int scale = Random::GenerateInt(128, 172);
		particle.size = particle.sSize = scale / 8;
		particle.dSize = scale;
	}
}
