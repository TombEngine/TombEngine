#include "framework.h"
#include "Objects/TR3/Entity/PunaBoss.h"

#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/item_fx.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Effects/Boss.h"
#include "Specific/level.h"

using namespace TEN::Effects::Boss;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Items;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto PUNA_LIGHTNING_DAMAGE = 350;

	constexpr auto PUNA_ATTACK_RANGE = BLOCK(20);
	constexpr auto PUNA_ALERT_RANGE	 = BLOCK(2.5f);

	constexpr auto PUNA_TURN_RATE_MAX			 = ANGLE(3.0f);
	constexpr auto PUNA_CHAIR_TURN_RATE_MAX		 = ANGLE(7.0f);
	constexpr auto PUNA_HEAD_X_ANGLE_MAX		 = ANGLE(20.0f);
	constexpr auto PUNA_ADJUST_LIGHTNING_X_ANGLE = ANGLE(3.0f);

	constexpr auto PUNA_EXPLOSION_NUM_MAX	= 60;
	constexpr auto PUNA_HEAD_ATTACK_NUM_MAX = 4;

	constexpr auto PUNA_EFFECT_COLOR		   = Vector4(0.0f, 0.4f, 0.5f, 0.5f);
	constexpr auto PUNA_EXPLOSION_MAIN_COLOR   = Vector4(0.0f, 0.7f, 0.3f, 0.5f);
	constexpr auto PUNA_EXPLOSION_SECOND_COLOR = Vector4(0.1f, 0.3f, 0.7f, 0.5f);

	const auto PunaBossHeadBite = CreatureBiteInfo(Vector3::Zero, 8);
	const auto PunaBossHandBite = CreatureBiteInfo(Vector3::Zero, 14);

	enum PunaState
	{
		PUNA_STATE_IDLE = 0,
		PUNA_STATE_HEAD_ATTACK = 1,
		PUNA_STATE_HAND_ATTACK = 2,
		PUNA_STATE_DEATH = 3
	};

	enum PunaAnim
	{
		PUNA_ANIM_IDLE = 0,
		PUNA_ANIM_HEAD_ATTACK = 1,
		PUNA_ANIM_HAND_ATTACK = 2,
		PUNA_ANIM_DEATH = 3
	};

	enum class PunaAttackType
	{
		AwaitPlayer,
		DeathLightning,
		SummonLightning,
		Wait // Used while an active lizard is nearby.
	};

	static short GetPunaHeadOrientToTarget(ItemInfo& item, const Vector3& target)
	{
		if (!item.TestFlags((int)BossItemFlags::Object, (short)BossFlagValue::Lizard))
			return NO_VALUE;

		auto pos = GetJointPosition(&item, PunaBossHeadBite).ToVector3();
		auto orient = Geometry::GetOrientToPoint(pos, target);
		return (orient.y - item.Pose.Orientation.y);
	}

	static std::vector<int> GetLizardEntityList(const ItemInfo& item)
	{
		auto entityList = std::vector<int>{};

		for (auto& currentEntity : g_Level.Items)
		{
			if (currentEntity.ObjectNumber == ID_LIZARD &&
				currentEntity.RoomNumber == item.RoomNumber &&
				currentEntity.HitPoints > 0 &&
				currentEntity.Status == ITEM_INVISIBLE &&
				!(currentEntity.Flags & IFLAG_KILLED))
			{
				entityList.push_back(currentEntity.Index);
			}
		}

		return entityList;
	}

	static Vector3 GetLizardTargetPosition(ItemInfo& item)
	{
		if (!item.TestFlagField((int)BossItemFlags::ItemNumber, NO_VALUE))
		{
			const auto& targetEntity = g_Level.Items[item.GetFlagField((int)BossItemFlags::ItemNumber)];
			return targetEntity.Pose.Position.ToVector3();
		}

		// Failsafe.
		const auto& creature = *GetCreatureInfo(&item);
		return creature.Target.ToVector3();
	}

	static int GetLizardItemNumber(const ItemInfo& item)
	{
		if (!item.TestFlags((int)BossItemFlags::Object, (short)BossFlagValue::Lizard))
			return NO_VALUE;

		auto lizardList = GetLizardEntityList(item);
		if (lizardList.empty())
			return NO_VALUE;

		if (lizardList.size() == 1)
			return lizardList[0];
		else
			return lizardList[Random::GenerateInt(0, (int)lizardList.size() - 1)];
	}

	static bool IsLizardActiveNearby(const ItemInfo& item, bool isInitializing = false)
	{
		for (auto& currentEntity : g_Level.Items)
		{
			// Check if the entity is a lizard.
			if (currentEntity.ObjectNumber != ID_LIZARD)
				continue;

			// Check if the entity is in the same room as Puna.
			if (currentEntity.RoomNumber != item.RoomNumber)
				continue;

			// If the entity is currently initializing, return early.
			if (isInitializing)
				return true;

			// Check status of entity.
			if (currentEntity.HitPoints > 0 &&
				currentEntity.Status == ITEM_INVISIBLE &&
				!(currentEntity.Flags & IFLAG_KILLED))
			{
				return true;
			}
		}

		return false;
	}

	static void SpawnSummonSmoke(const Vector3& pos)
	{
		auto& smoke = *GetFreeParticle();
		
		int scale = Random::GenerateInt(256, 384);

		smoke.on = true;
		smoke.SpriteSeqID = ID_DEFAULT_SPRITES;
		smoke.SpriteID = 0;
		smoke.blendMode = BlendMode::Additive;
		smoke.x = pos.x + Random::GenerateInt(-64, 64);
		smoke.y = pos.y - Random::GenerateInt(0, 32);
		smoke.z = pos.z + Random::GenerateInt(-64, 64);
		smoke.xVel = Random::GenerateInt(-128, 128);
		smoke.yVel = Random::GenerateInt(-32, -16);
		smoke.zVel = Random::GenerateInt(-128, 128);
		smoke.sR = 16;
		smoke.sG = 64;
		smoke.sB = 0;
		smoke.dR = 8;
		smoke.dG = 32;
		smoke.dB = 0;
		smoke.colFadeSpeed = Random::GenerateInt(16, 24);
		smoke.fadeToBlack = 64;
		smoke.sLife =
		smoke.life = Random::GenerateInt(96, 112);

		smoke.extras = 0;
		smoke.dynamic = -1;
		smoke.friction = 0;

		if (Random::TestProbability(1 / 2.0f))
		{
			smoke.rotAng = Random::GenerateAngle();
			smoke.rotAdd = Random::GenerateAngle(ANGLE(-0.06f), ANGLE(0.06f));
			smoke.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_WIND;
		}
		else
		{
			smoke.flags = SP_SCALE | SP_DEF | SP_EXPDEF | SP_WIND;
		}

		smoke.scalar = 3;
		smoke.gravity = Random::GenerateInt(-16, -8);
		smoke.maxYvel = Random::GenerateInt(-12, -8);
		smoke.size =
		smoke.sSize = scale / 2;
		smoke.dSize = scale;
	}

	static void SpawnLizard(ItemInfo& item)
	{
		if (!item.TestFlagField((int)BossItemFlags::ItemNumber, NO_VALUE))
		{
			auto itemNumber = item.GetFlagField((int)BossItemFlags::ItemNumber);
			auto& currentItem = g_Level.Items[itemNumber];

			for (int i = 0; i < 20; i++)
				SpawnSummonSmoke(currentItem.Pose.Position.ToVector3());

			AddActiveItem(itemNumber);
			currentItem.ItemFlags[0] = 1; // Flag 1 = spawned lizard.
			item.SetFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::Wait);
		}
	}

	static void SpawnPunaLightning(ItemInfo& item, const Vector3& pos, const CreatureBiteInfo& bite, bool isSummon)
	{
		const auto& creature = *GetCreatureInfo(&item);

		auto origin = GameVector(GetJointPosition(&item, bite), item.RoomNumber);

		if (isSummon)
		{
			auto target = GameVector(pos, item.RoomNumber);

			SpawnElectricity(origin.ToVector3(), target.ToVector3(), 1, 0, 255, 180, 30, (int)(int)(int)ElectricityFlags::ThinIn | (int)ElectricityFlags::Spline | (int)ElectricityFlags::MoveEnd, 8, 12);
			SpawnElectricity(origin.ToVector3(), target.ToVector3(), 1, 180, 255, 0, 30, (int)(int)(int)ElectricityFlags::ThinIn | (int)ElectricityFlags::Spline | (int)ElectricityFlags::MoveEnd, 3, 12);
			SpawnElectricity(origin.ToVector3(), target.ToVector3(), Random::GenerateInt(25, 50), 100, 200, 200, 30, (int)(int)(int)ElectricityFlags::ThinIn | (int)(int)ElectricityFlags::ThinOut, 4, 12);
			SpawnElectricity(origin.ToVector3(), target.ToVector3(), Random::GenerateInt(25, 50), 100, 250, 255, 30, (int)(int)(int)ElectricityFlags::ThinIn | (int)(int)ElectricityFlags::ThinOut, 2, 12);

			SpawnDynamicLight(origin.x, origin.y, origin.z, 20, 0, 255, 0);
			SpawnLizard(item);
		}
		else
		{
			auto target = GameVector(Geometry::TranslatePoint(origin.ToVector3(), pos - origin.ToVector3(), PUNA_ATTACK_RANGE), creature.Enemy->RoomNumber);

			auto origin1 = GameVector(Geometry::TranslatePoint(origin.ToVector3(), pos - origin.ToVector3(), PUNA_ATTACK_RANGE / 4), creature.Enemy->RoomNumber);
			auto origin2 = GameVector(Geometry::TranslatePoint(origin1.ToVector3(), pos - origin1.ToVector3(), PUNA_ATTACK_RANGE / 4), creature.Enemy->RoomNumber);

			auto target2 = GameVector(Geometry::TranslatePoint(origin.ToVector3(), pos - origin.ToVector3(), PUNA_ATTACK_RANGE / 6), creature.Enemy->RoomNumber);
			auto target3 = GameVector(Geometry::TranslatePoint(origin1.ToVector3(), pos - origin1.ToVector3(), PUNA_ATTACK_RANGE / 10), creature.Enemy->RoomNumber);

			SpawnElectricity(origin.ToVector3(), target2.ToVector3(), Random::GenerateInt(15, 40), 20, 160, 160, 20, (int)(int)ElectricityFlags::ThinOut | (int)(int)(int)ElectricityFlags::ThinIn, 4, 6);
			SpawnElectricity(origin.ToVector3(), target2.ToVector3(), Random::GenerateInt(25, 35), 20, 160, 160, 20, (int)(int)ElectricityFlags::ThinOut | (int)(int)(int)ElectricityFlags::ThinIn, 2, 7);

			SpawnElectricity(target2.ToVector3(), origin1.ToVector3(), Random::GenerateInt(15, 40), 20, 160, 160, 20, (int)(int)ElectricityFlags::ThinOut | (int)(int)(int)ElectricityFlags::ThinIn, 4, 6);
			SpawnElectricity(target2.ToVector3(), origin1.ToVector3(), Random::GenerateInt(25, 35), 20, 160, 160, 20, (int)(int)ElectricityFlags::ThinOut | (int)(int)(int)ElectricityFlags::ThinIn, 2, 7);

			SpawnElectricity(origin1.ToVector3(), target3.ToVector3(), Random::GenerateInt(15, 40), 20, 160, 160, 20, (int)(int)ElectricityFlags::ThinOut | (int)(int)(int)ElectricityFlags::ThinIn, 4, 9);
			SpawnElectricity(origin1.ToVector3(), target3.ToVector3(), Random::GenerateInt(25, 35), 20, 160, 160, 20, (int)(int)ElectricityFlags::ThinOut | (int)(int)(int)ElectricityFlags::ThinIn, 2, 10);

			SpawnElectricity(origin2.ToVector3(), target3.ToVector3(), Random::GenerateInt(15, 40), 20, 160, 160, 16, (int)(int)ElectricityFlags::ThinOut | (int)(int)(int)ElectricityFlags::ThinIn, 4, 7);
			SpawnElectricity(origin2.ToVector3(), target3.ToVector3(), Random::GenerateInt(25, 35), 20, 160, 160, 16, (int)(int)ElectricityFlags::ThinOut | (int)(int)(int)ElectricityFlags::ThinIn, 2, 8);

			SpawnElectricity(origin.ToVector3(), target.ToVector3(), 1, 20, 160, 160, 30, (int)(int)(int)ElectricityFlags::ThinIn | (int)ElectricityFlags::Spline | (int)ElectricityFlags::MoveEnd, 12, 12);
			SpawnElectricity(origin.ToVector3(), target.ToVector3(), 1, 80, 160, 160, 30, (int)(int)(int)ElectricityFlags::ThinIn | (int)ElectricityFlags::Spline | (int)ElectricityFlags::MoveEnd, 5, 12);

			SpawnDynamicLight(origin.x, origin.y, origin.z, 20, 0, 255, 255);

			auto hitPos = Vector3i::Zero;
			if (ObjectOnLOS2(&origin, &target, &hitPos, nullptr, ID_LARA) == creature.Enemy->Index)
			{
				if (creature.Enemy->HitPoints <= PUNA_LIGHTNING_DAMAGE)
				{
					ItemElectricBurn(creature.Enemy);
					DoDamage(creature.Enemy, PUNA_LIGHTNING_DAMAGE);
				}
				else
				{
					DoDamage(creature.Enemy, PUNA_LIGHTNING_DAMAGE);
				}
			}
		}
	}

	void InitializePuna(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, PUNA_ANIM_IDLE);
		CheckForRequiredObjects(item);

		// Save Puna's default angle. It will be used while waiting (i.e. an active lizard is nearby).
		// NOTE: Since Puna is oriented to face away from the player, add 180 degrees.
		item.SetFlagField((int)BossItemFlags::Rotation, item.Pose.Orientation.y + ANGLE(180.0f));
		item.SetFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::AwaitPlayer); // Normal behaviour at start.
		item.SetFlagField((int)BossItemFlags::ShieldIsEnabled, 1);							 // Activated at start.
		item.SetFlagField((int)BossItemFlags::AttackCount, 0);
		item.SetFlagField((int)BossItemFlags::DeathCount, 0);
		item.SetFlagField((int)BossItemFlags::ItemNumber, NO_VALUE);
		item.SetFlagField((int)BossItemFlags::ExplodeCount, 0);

		// If there is no lizard nearby, remove the lizard flag.
		if (!IsLizardActiveNearby(item, true))
			item.ClearFlags((int)BossItemFlags::Object, (short)BossFlagValue::Lizard);
	}

	void PunaControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;
		
		auto& item = g_Level.Items[itemNumber];
		auto& object = Objects[item.ObjectNumber];
		auto& creature = *GetCreatureInfo(&item);

		static auto targetPos = Vector3i::Zero;

		auto headOrient = EulerAngles::Identity;
		short headingAngle = 0;
		short prevYOrient = 0;

		bool hasTurned = false;
		bool isLizardActiveNearby = IsLizardActiveNearby(item);

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != PUNA_STATE_DEATH)
			{
				SetAnimation(item, PUNA_ANIM_DEATH);
				SoundEffect(SFX_TR3_PUNA_BOSS_DEATH, &item.Pose);
				item.ItemFlags[(int)BossItemFlags::DeathCount] = 1;
				creature.MaxTurn = 0;
			}

			auto deathCount = item.GetFlagField((int)BossItemFlags::DeathCount);
			item.Pose.Orientation.z = (Random::GenerateInt() % deathCount) - (item.ItemFlags[(int)BossItemFlags::DeathCount] >> 1);

			if (deathCount < 2048)
				item.ItemFlags[(int)BossItemFlags::DeathCount] += 32;

			int endFrameNumber = GetAnimData(object, PUNA_ANIM_DEATH).EndFrameNumber;
			if (item.Animation.FrameNumber >= endFrameNumber)
			{
				// Avoid having the object stop working.
				item.Animation.FrameNumber = endFrameNumber;

				if (item.GetFlagField((int)BossItemFlags::ExplodeCount) < PUNA_EXPLOSION_NUM_MAX)
					item.ItemFlags[(int)BossItemFlags::ExplodeCount]++;

				if (item.ItemFlags[7] < PUNA_EXPLOSION_NUM_MAX)
					item.ItemFlags[7]++;

				// Do explosion effect.
				ExplodeBoss(itemNumber, item, PUNA_EXPLOSION_NUM_MAX, PUNA_EFFECT_COLOR, PUNA_EXPLOSION_MAIN_COLOR, PUNA_EXPLOSION_SECOND_COLOR);
				return;
			}
		}
		else
		{
			prevYOrient = item.Pose.Orientation.y;

			AI_INFO ai;
			CreatureAIInfo(&item, &ai);
			if (ai.ahead)
			{
				headOrient.x = ai.xAngle;
				headOrient.y = ai.angle;
			}

			if (item.TestFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::AwaitPlayer) && creature.Enemy != nullptr)
			{
				float distance = Vector3i::Distance(creature.Enemy->Pose.Position, item.Pose.Position);

				if (distance <= BLOCK(2.5f))
					item.SetFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::DeathLightning);

				// Rotate the object on puna boss chair.
				creature.JointRotation[0] += PUNA_CHAIR_TURN_RATE_MAX;
				return;
			}

			// Get target.
			if (item.TestFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::DeathLightning))
			{
				creature.Target = creature.Enemy->Pose.Position;
			}
			else if (item.TestFlags((int)BossItemFlags::Object, (short)BossFlagValue::Lizard) &&
				item.TestFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::SummonLightning) &&
				item.TestFlagField((int)BossItemFlags::ItemNumber, NO_VALUE) &&
				!item.TestFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::Wait) && isLizardActiveNearby)
			{
				// Get random lizard item number.
				item.SetFlagField((int)BossItemFlags::ItemNumber, (short)GetLizardItemNumber(item));
				creature.Target = GetLizardTargetPosition(item);
			}
			else if (item.TestFlags((int)BossItemFlags::Object, (short)BossFlagValue::Lizard) &&
				item.TestFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::Wait) &&
				!item.TestFlagField((int)BossItemFlags::ItemNumber, NO_VALUE))
			{
				// Rotate to idle position while player fights lizard.
				auto targetOrient = EulerAngles(item.Pose.Orientation.x, item.GetFlagField((int)BossItemFlags::Rotation), item.Pose.Orientation.z);
				item.Pose.Orientation.InterpolateConstant(targetOrient, ANGLE(3.0f));

				// Check if target is dead.
				auto& summonItem = g_Level.Items[item.GetFlagField((int)BossItemFlags::ItemNumber)];

				if (summonItem.HitPoints <= 0)
				{
					// Reset the attack type, attack count, itemNumber, and restart the sequence.
					item.SetFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::DeathLightning);
					item.SetFlagField((int)BossItemFlags::AttackCount, 0);
					item.SetFlagField((int)BossItemFlags::ItemNumber, NO_VALUE);
				}
			}

			short headYOrient = GetPunaHeadOrientToTarget(item, creature.Target.ToVector3());
			headingAngle = CreatureTurn(&item, creature.MaxTurn);

			switch (item.Animation.ActiveState)
			{
			case PUNA_STATE_IDLE:
				creature.MaxTurn = PUNA_TURN_RATE_MAX;
				item.SetFlagField((int)BossItemFlags::ShieldIsEnabled, 1);

				if ((item.Animation.TargetState != PUNA_STATE_HAND_ATTACK && item.Animation.TargetState != PUNA_STATE_HEAD_ATTACK) &&
					ai.angle > ANGLE(-1.0f) && ai.angle < ANGLE(1.0f) &&
					creature.Enemy->HitPoints > 0 &&
					item.GetFlagField((int)BossItemFlags::AttackCount) < PUNA_HEAD_ATTACK_NUM_MAX &&
					!item.TestFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::SummonLightning) && !item.TestFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::Wait))
				{
					creature.MaxTurn = 0;
					targetPos = creature.Target;
					targetPos.y -= CLICK(2);
					item.SetFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::DeathLightning);

					if (Random::TestProbability(1 / 3.0f))
						item.Animation.TargetState = PUNA_STATE_HEAD_ATTACK;
					else
						item.Animation.TargetState = PUNA_STATE_HAND_ATTACK;

					if (item.TestFlags((int)BossItemFlags::Object, (short)BossFlagValue::Lizard) && isLizardActiveNearby)
						item.ItemFlags[(int)BossItemFlags::AttackCount]++;
				}
				else if (item.ItemFlags[(int)BossItemFlags::AttackCount] >= PUNA_HEAD_ATTACK_NUM_MAX &&
					creature.Enemy->HitPoints > 0 && 
					item.ItemFlags[(int)BossItemFlags::AttackType] != (int)PunaAttackType::Wait)
				{
					item.SetFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::SummonLightning);

					if (!item.TestFlagField((int)BossItemFlags::ItemNumber, NO_VALUE))
					{
						if (headYOrient > ANGLE(-1.0f) && headYOrient < ANGLE(1.0f))
						{
							targetPos = creature.Target;
							targetPos.y -= CLICK(2);
							item.Animation.TargetState = PUNA_STATE_HAND_ATTACK;
						}
					}
				}

				break;

			case PUNA_STATE_HEAD_ATTACK:
				item.SetFlagField((int)BossItemFlags::ShieldIsEnabled, 0);
				creature.MaxTurn = 0;

				if (item.Animation.FrameNumber == 14)
					SpawnPunaLightning(item, targetPos.ToVector3(), PunaBossHeadBite, false);

				break;

			case PUNA_STATE_HAND_ATTACK:
				item.SetFlagField((int)BossItemFlags::ShieldIsEnabled, 0);
				creature.MaxTurn = 0;

				if (item.Animation.FrameNumber == 30)
				{
					if (item.TestFlags((int)BossItemFlags::Object, (short)BossFlagValue::Lizard) &&
						item.TestFlagField((int)BossItemFlags::AttackType, (int)PunaAttackType::SummonLightning) &&
						!item.TestFlagField((int)BossItemFlags::ItemNumber, NO_VALUE) && isLizardActiveNearby)
					{
						SpawnPunaLightning(item, targetPos.ToVector3(), PunaBossHandBite, true);
					}
					else
					{
						SpawnPunaLightning(item, targetPos.ToVector3(), PunaBossHandBite, false);
					}
				}

				break;
			}
		}

		// Rotate chair.
		creature.JointRotation[0] += PUNA_CHAIR_TURN_RATE_MAX;

		CreatureJoint(&item, 1, headOrient.y);
		CreatureJoint(&item, 2, headOrient.x, PUNA_HEAD_X_ANGLE_MAX);
		CreatureAnimation(itemNumber, headingAngle, 0);

		// Emit sound while chair is rotating.
		if (prevYOrient != item.Pose.Orientation.y && !hasTurned)
		{
			hasTurned = true;
			SoundEffect(SFX_TR3_PUNA_BOSS_TURN_CHAIR, &item.Pose);
		}
		else if (prevYOrient == item.Pose.Orientation.y)
		{
			hasTurned = false;
			StopSoundEffect(SFX_TR3_PUNA_BOSS_CHAIR_2);
			StopSoundEffect(SFX_TR3_PUNA_BOSS_TURN_CHAIR);
		}
	}

	void PunaHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		if (target.TestFlags((int)BossItemFlags::Object, (short)BossFlagValue::Shield) &&
			target.TestFlagField((int)BossItemFlags::ShieldIsEnabled, 1))
		{
			auto color = Vector4(
				0.0f,
				Random::GenerateFloat(0.0f, 0.5f),
				Random::GenerateFloat(0.0f, 0.5f),
				Random::GenerateFloat(0.5f, 0.8f));

			if (pos.has_value() && !isExplosive)
			{
				SpawnShieldAndRichochetSparks(target, pos->ToVector3(), color);
			}
			else if (isExplosive)
			{
				SpawnShield(target, color);
			}
		}
		else
		{
			if (target.HitStatus)
				SoundEffect(SFX_TR3_PUNA_BOSS_TAKE_HIT, &target.Pose);

			if (pos.has_value())
				DoBloodSplat(pos->x, pos->y, pos->z, 5, source.Pose.Orientation.y, pos->RoomNumber);

			DoItemHit(&target, damage, isExplosive, false);
		}
	}
}
