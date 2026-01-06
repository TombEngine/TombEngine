#include "framework.h"
#include "Objects/TR2/Entity/tr2_sword_guardian.h"

#include "Game/collision/collide_room.h"
#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Objects/TR2/Entity/tr2_spear_guardian.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;

namespace TEN::Entities::Creatures::TR2
{
	constexpr auto SWORD_GUARDIAN_ATTACK_DAMAGE = 300;

	// TODO: Fix names.
	constexpr auto SWORD_GUARDIAN_ATTACK_RANGE		= SQUARE(BLOCK(1));
	constexpr auto SWORD_GUARDIAN_WALK_ATTACK_RANGE = SQUARE(BLOCK(2));

	constexpr auto SWORD_GUARDIAN_FLY_TURN_RATE_MAX	 = ANGLE(7.0f);
	constexpr auto SWORD_GUARDIAN_WALK_TURN_RATE_MAX = ANGLE(9.0f);

	constexpr auto SWORD_GUARDIAN_MESH_SWAP_TIME = 3;

	const auto SwordBite = CreatureBiteInfo(Vector3(0, 37, 550), 15);

	enum SwordGuardianState
	{
		SWORD_GUARDIAN_STATE_IDLE = 1,
		SWORD_GUARDIAN_STATE_WALK = 2,
		SWORD_GUARDIAN_STATE_ATTACK_FRONT_AIM = 3,
		SWORD_GUARDIAN_STATE_ATTACK_FRONT = 4,
		SWORD_GUARDIAN_STATE_ATTACK_HORIZONTAL_AIM = 5,
		SWORD_GUARDIAN_STATE_ATTACK_HORIZONTAL = 6,
		SWORD_GUARDIAN_STATE_WAIT = 7,
		SWORD_GUARDIAN_STATE_FLY = 8,
		SWORD_GUARDIAN_STATE_AWAKE = 9,
		SWORD_GUARDIAN_STATE_WALK_ATTACK_AIM = 10,
		SWORD_GUARDIAN_STATE_WALK_ATTACK = 11,
		SWORD_GUARDIAN_STATE_DEATH = 12
	};

	// TODO: Determine names.
	enum SwordGuardianAnim
	{
		SWORD_GUARDIAN_ANIM_AWAKE = 0
	};

	static void SpawnSwordGuardianEffect(const Vector3& pos, int roomNumber)
	{
		auto& particle = *GetFreeParticle();

		bool isUnderwater = TestEnvironment(ENV_FLAG_WATER, Vector3i(pos), roomNumber);
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
		particle.sR = 0;
		particle.sG = 0;
		particle.sB = 0;
		particle.dR = 255;
		particle.dG = 255;
		particle.dB = 255;
		particle.colFadeSpeed = 8;
		particle.fadeToBlack = 64;
		particle.sLife =
		particle.life = Random::GenerateInt(72, 128);
		particle.extras = 0;
		particle.dynamic = -1;
		particle.friction = 6;
		particle.rotAng = Random::GenerateAngle();
		particle.rotAdd = Random::GenerateAngle(ANGLE(-0.2f), ANGLE(0.2f));
		particle.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
		particle.scalar = 3;
		particle.gravity = Random::GenerateInt(-8, -4);
		particle.maxYvel = Random::GenerateInt(-8, -4);

		int scale = Random::GenerateInt(100, 132);
		particle.size =
		particle.sSize = scale / 8;
		particle.dSize = scale;
	}

	static void MeshSwapNormalToJade(ItemInfo& item, int jointIndex, bool doEffect = true)
	{
		if (doEffect)
			SpawnSpearGuardianEffect(GetJointPosition(&item, jointIndex).ToVector3(), item.RoomNumber);

		item.Model.MeshIndex[jointIndex] = Objects[ID_SWORD_GUARDIAN_STATUE].meshIndex + jointIndex;
	}

	static void MeshSwapJadeToNormal(ItemInfo& item, int jointIndex, bool doEffect = true)
	{
		if (doEffect)
			SpawnSpearGuardianEffect(GetJointPosition(&item, jointIndex).ToVector3(), item.RoomNumber);

		item.Model.MeshIndex[jointIndex] = Objects[ID_SWORD_GUARDIAN].meshIndex + jointIndex;
	}

	static bool DoSwordGuardianMeshSwap(ItemInfo& item)
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

			creature.Flags = SWORD_GUARDIAN_MESH_SWAP_TIME;
		}
		else
		{
			creature.Flags--;
		}

		return false;
	}

	static void DoSwordGuardianFlyEffect(ItemInfo* item)
	{
		auto sphere = BoundingSphere(item->Pose.Position.ToVector3(), 1.0f);
		auto pos = Random::GeneratePointInSphere(sphere);
		SpawnSwordGuardianEffect(pos, item->RoomNumber);
		SoundEffect(SFX_TR2_WARRIOR_HOVER, &item->Pose);
	}

	void InitializeSwordGuardian(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		const auto& object = Objects[item.ObjectNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, SWORD_GUARDIAN_ANIM_AWAKE);
		item.Status = ITEM_NOT_ACTIVE;

		item.ItemFlags[0] = 0; // Joint index for mesh swap.
		item.ItemFlags[1] = 1; // Immune state (bool).
		item.ItemFlags[2] = 1; // 1 = swap to jade. 2 = swap to normal.
		item.ItemFlags[3] = 0; // Set to true if mesh is swapped to jade, otherwise false.

		for (int jointIndex = 0; jointIndex < object.nmeshes; jointIndex++)
			MeshSwapNormalToJade(item, jointIndex, false);
	}

	void SwordGuardianControl(short itemNumber)
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
			if (item->Animation.ActiveState != SWORD_GUARDIAN_STATE_DEATH)
			{
				item->ItemFlags[3] = 1;
				item->ItemFlags[0] = object->nmeshes - 1;
				item->Animation.ActiveState = SWORD_GUARDIAN_STATE_DEATH;
			}

			if (DoSwordGuardianMeshSwap(*item))
				CreatureDie(itemNumber, true);

			return;
		}
		else
		{
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -CLICK(1);
			creature->LOT.Fly = NO_FLYING;
			creature->LOT.Zone = ZoneType::Basic;

			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			if (ai.enemyZone != ai.zoneNumber && item->Animation.ActiveState == SWORD_GUARDIAN_STATE_FLY)
			{
				creature->LOT.Step = BLOCK(20);
				creature->LOT.Drop = -BLOCK(20);
				creature->LOT.Fly = 64;
				creature->LOT.Zone = ZoneType::Flyer;
				CreatureAIInfo(item, &ai);
			}

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			if (ai.ahead &&
			   (item->Animation.ActiveState != SWORD_GUARDIAN_STATE_AWAKE &&
				   item->Animation.ActiveState != SWORD_GUARDIAN_STATE_FLY))
			{
				extraHeadRot.x = ai.xAngle / 2;
				extraHeadRot.y = ai.angle / 2;
			}

			switch (item->Animation.ActiveState)
			{
			case SWORD_GUARDIAN_STATE_AWAKE:
				creature->MaxTurn = 0;

				DoSwordGuardianMeshSwap(*item);
				break;

			case SWORD_GUARDIAN_STATE_IDLE:
				creature->MaxTurn = 0;
				item->ItemFlags[1] = 0; // Remove immunity.

				if (!isPlayerAlive)
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_WAIT;
				else if (ai.bite && ai.distance < SWORD_GUARDIAN_ATTACK_RANGE)
				{
					if (Random::TestProbability(1 / 2.0f))
						item->Animation.TargetState = SWORD_GUARDIAN_STATE_ATTACK_HORIZONTAL_AIM;
					else
						item->Animation.TargetState = SWORD_GUARDIAN_STATE_ATTACK_FRONT_AIM;
				}
				else if (ai.zoneNumber != ai.enemyZone)
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_FLY;
				else
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_WALK;

				break;

			case SWORD_GUARDIAN_STATE_WALK:
				creature->MaxTurn = SWORD_GUARDIAN_WALK_TURN_RATE_MAX;

				if (!isPlayerAlive)
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_IDLE;
				else if (ai.bite && ai.distance < SWORD_GUARDIAN_WALK_ATTACK_RANGE)
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_WALK_ATTACK_AIM;
				else if (ai.zoneNumber != ai.enemyZone)
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_IDLE;

				break;

			case SWORD_GUARDIAN_STATE_ATTACK_FRONT_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!ai.bite || ai.distance > SWORD_GUARDIAN_ATTACK_RANGE)
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_IDLE;
				else
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_ATTACK_FRONT;

				break;

			case SWORD_GUARDIAN_STATE_ATTACK_HORIZONTAL_AIM:
				creature->Flags = 0;
				
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!ai.bite || ai.distance > SWORD_GUARDIAN_ATTACK_RANGE)
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_IDLE;
				else
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_ATTACK_HORIZONTAL;

				break;

			case SWORD_GUARDIAN_STATE_WALK_ATTACK_AIM:
				creature->Flags = 0;

				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!ai.bite || ai.distance > SWORD_GUARDIAN_WALK_ATTACK_RANGE)
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_IDLE;
				else
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_WALK_ATTACK;

				break;

			case SWORD_GUARDIAN_STATE_FLY:
				creature->MaxTurn = SWORD_GUARDIAN_FLY_TURN_RATE_MAX;

				DoSwordGuardianFlyEffect(item);
				if (creature->LOT.Fly == NO_FLYING)
					item->Animation.TargetState = SWORD_GUARDIAN_STATE_IDLE;

				break;

			case SWORD_GUARDIAN_STATE_ATTACK_FRONT:
			case SWORD_GUARDIAN_STATE_ATTACK_HORIZONTAL:
			case SWORD_GUARDIAN_STATE_WALK_ATTACK:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (!creature->Flags && item->TouchBits.Test(SwordBite.BoneID))
				{
					DoDamage(creature->Enemy, SWORD_GUARDIAN_ATTACK_DAMAGE);
					CreatureEffect(item, SwordBite, DoBloodSplat);
					creature->Flags = 1;
				}

				break;
			}
		}

		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureJoint(item, 3, extraHeadRot.x);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}

	void SwordGuardianHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		if (target.ItemFlags[1] == 1)
		{
			if (pos.has_value())
				TriggerRicochetSpark(pos.value(), source.Pose.Orientation.y);
			return;
		}

		DefaultItemHit(target, source, pos, damage, isExplosive, jointIndex);
	}
}
