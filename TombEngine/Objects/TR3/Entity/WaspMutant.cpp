#include "framework.h"
#include "Objects/TR3/Entity/WaspMutant.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/effects/spark.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"

using namespace TEN::Effects::Spark;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto WASP_DAMAGE = 50;

	constexpr auto WASP_DETECTION_RANGE = SQUARE(BLOCK(15));
	constexpr auto WASP_ATTACK_RANGE	= SQUARE(BLOCK(0.5f));

	constexpr auto WASP_LAND_CHANCE = 1 / 256.0f;

	constexpr auto WASP_LAND_TURN_RATE_MAX = ANGLE(1.0f);
	constexpr auto WASP_AIR_TURN_RATE_MAX  = ANGLE(3.0f);

	constexpr auto WASP_IDLE_TO_FLY_VELOCITY = BLOCK(1 / 20.0f);
	constexpr auto WASP_VENOM_SACK_LIGHT_POWER = 10;

	constexpr auto WaspVenomSackLightColor = Vector4(0.0f, 0.35f, 0.0f, 1.0f);

	const auto WaspBite			 = CreatureBiteInfo(Vector3(0, 0, -260), 12);
	const auto WaspVenomSackBite = CreatureBiteInfo(Vector3::Zero, 10);

	enum WaspMutantState
	{
		WASP_STATE_FLY_IDLE,
		WASP_STATE_FLY_IDLE_TO_IDLE,
		WASP_STATE_IDLE,
		WASP_STATE_IDLE_TO_FLY_IDLE,
		WASP_STATE_ATTACK,
		WASP_STATE_DEATH_START,
		WASP_STATE_DEATH_END,
		WASP_STATE_FLY_FORWARD
	};

	enum WaspMutantAnim
	{
		WASP_ANIM_FLY_IDLE,
		WASP_ANIM_FLY_IDLE_TO_IDLE,
		WASP_ANIM_IDLE,
		WASP_ANIM_IDLE_TO_FLY_IDLE,
		WASP_ANIM_ATTACK,
		WASP_ANIM_DEATH_START,
		WASP_ANIM_DEATH_END,
		WASP_ANIM_FLY_FORWARD
	};

	static void SpawnWaspMutantVenomSackParticle(int itemNumber)
	{
		constexpr auto PARTICLE_SIZE_MAX = 8.0f;
		constexpr auto PARTICLE_SIZE_MIN = PARTICLE_SIZE_MAX / 2;

		auto& particle = *GetFreeParticle();

		auto sphere = BoundingSphere(Vector3::Zero, BLOCK(1 / 32.0f));
		auto pos = Random::GeneratePointInSphere(sphere);

		particle.on = true;
		particle.SpriteSeqID = ID_DEFAULT_SPRITES;
		particle.SpriteID = 0;
		particle.blendMode = BlendMode::Additive;
		particle.fxObj = itemNumber;
		particle.nodeNumber = ParticleNodeOffsetIDs::NodeWasp;

		particle.sG = Random::GenerateInt(32, 96);
		particle.sB = particle.sG / 2;
		particle.sR = particle.sG / 4;
		particle.dG = Random::GenerateInt(224, 256);
		particle.dB = particle.dG / 2;
		particle.dR = particle.dG / 4;
		
		particle.colFadeSpeed = 4;
		particle.fadeToBlack = 2;
		particle.sLife =
		particle.life = 8;

		particle.extras = 0;
		particle.dynamic = -1;

		particle.x = pos.x;
		particle.y = pos.y;
		particle.z = pos.z;

		particle.xVel = Random::GenerateInt(-32, 32);
		particle.yVel = Random::GenerateInt(-32, 32);
		particle.zVel = Random::GenerateInt(-32, 32);
		particle.friction = 2 | (2 << 4);

		particle.flags = SP_SCALE | SP_ITEM | SP_NODEATTACH | SP_DEF;
		particle.gravity = particle.maxYvel = 0;
		particle.scalar = 3;

		particle.size =
		particle.sSize = Random::GenerateFloat(PARTICLE_SIZE_MIN, PARTICLE_SIZE_MAX);
		particle.dSize = particle.sSize / 2;
	}

	static void SpawnWaspMutantVenomSackEffects(ItemInfo& item, int itemNumber)
	{
		constexpr auto PARTICLE_EFFECT_COUNT = 2;

		if (item.ItemFlags[0] < 0)
			item.ItemFlags[0] = 0;

		// Spawn light.
		auto pos = GetJointPosition(&item, WaspVenomSackBite);
		SpawnDynamicLight(
			pos.x, pos.y, pos.z,
			item.ItemFlags[0],
			WaspVenomSackLightColor.x * UCHAR_MAX,
			WaspVenomSackLightColor.y * UCHAR_MAX,
			WaspVenomSackLightColor.z * UCHAR_MAX);

		// Spawn particles.
		for (int i = 0; i < PARTICLE_EFFECT_COUNT; i++)
			SpawnWaspMutantVenomSackParticle(itemNumber);
	}

	void InitializeWaspMutant(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, WASP_STATE_IDLE);
		item.ItemFlags[0] = WASP_VENOM_SACK_LIGHT_POWER;
	}

	void WaspMutantControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;

		if (item.HitPoints <= 0)
		{
			switch (item.Animation.ActiveState)
			{
			case WASP_STATE_DEATH_START:
				if (item.Pose.Position.y >= item.Floor)
				{
					item.Animation.TargetState = WASP_STATE_DEATH_END;
					item.Animation.IsAirborne = false;
					item.Animation.Velocity.y = 0.0f;
					item.Pose.Position.y = item.Floor;
				}

				break;

			case WASP_STATE_DEATH_END:
				if (item.ItemFlags[0] > 0 && item.ItemFlags[1] == 0)
				{
					item.ItemFlags[0]--;
					item.ItemFlags[1] = 6;
				}
				else
				{
					item.ItemFlags[1]--;
				}

				item.Pose.Position.y = item.Floor;
				break;

			default:
				SetAnimation(item, WASP_STATE_DEATH_START);
				item.Animation.IsAirborne = true;
				item.Animation.Velocity = Vector3::Zero;
				break;
			}

			item.Pose.Orientation.x = 0;
		}
		else
		{
			if (item.AIBits)
				GetAITarget(&creature);

			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			GetCreatureMood(&item, &ai, true);
			CreatureMood(&item, &ai, true);

			headingAngle = CreatureTurn(&item, creature.MaxTurn);

			switch (item.Animation.ActiveState)
			{
			case WASP_STATE_IDLE:
				item.Pose.Position.y = item.Floor;
				creature.MaxTurn = WASP_LAND_TURN_RATE_MAX;

				if (item.HitStatus || ai.distance < WASP_DETECTION_RANGE || creature.HurtByLara)
					item.Animation.TargetState = WASP_STATE_IDLE_TO_FLY_IDLE;

				break;

			case WASP_STATE_FLY_IDLE_TO_IDLE:
				creature.MaxTurn = WASP_LAND_TURN_RATE_MAX;

				item.Pose.Position.y += WASP_IDLE_TO_FLY_VELOCITY;
				if (item.Pose.Position.y >= item.Floor)
					item.Pose.Position.y = item.Floor;

				break;

			case WASP_STATE_FLY_IDLE:
				creature.MaxTurn = WASP_AIR_TURN_RATE_MAX;
				creature.Flags = 0;

				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (ai.ahead && ai.distance < WASP_ATTACK_RANGE)
				{
					item.Animation.TargetState = WASP_STATE_ATTACK;
				}
				else if ((creature.Mood == MoodType::Bored || GetRandomControl() < WASP_LAND_CHANCE) &&
					!creature.HurtByLara)
				{
					item.Animation.TargetState = WASP_STATE_FLY_IDLE_TO_IDLE;
				}
				else
				{
					item.Animation.TargetState = WASP_STATE_FLY_FORWARD;
				}

				break;

			case WASP_STATE_FLY_FORWARD:
				creature.MaxTurn = WASP_AIR_TURN_RATE_MAX;
				creature.Flags = 0;

				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (ai.ahead && ai.distance < WASP_ATTACK_RANGE)
				{
					item.Animation.TargetState = WASP_STATE_ATTACK;
				}
				else if ((creature.Mood == MoodType::Bored || GetRandomControl() < WASP_LAND_CHANCE) &&
					!creature.HurtByLara)
				{
					item.Animation.TargetState = WASP_STATE_FLY_IDLE;
				}

				break;

			case WASP_STATE_ATTACK:
				creature.MaxTurn = WASP_AIR_TURN_RATE_MAX;

				if (ai.ahead && ai.distance < WASP_ATTACK_RANGE)
				{
					item.Animation.TargetState = WASP_STATE_ATTACK;
				}
				else if (ai.distance < WASP_ATTACK_RANGE)
				{
					item.Animation.TargetState = WASP_STATE_FLY_IDLE;
				}
				else
				{
					item.Animation.TargetState = WASP_STATE_FLY_IDLE;
					item.Animation.RequiredState = WASP_STATE_FLY_FORWARD;
				}

				if (!creature.Flags && item.TouchBits.Test(WaspBite.BoneID))
				{
					DoDamage(creature.Enemy, WASP_DAMAGE);
					CreatureEffect2(&item, WaspBite, 10, item.Pose.Orientation.y, DoBloodSplat);
					creature.Flags = 1;
				}

				break;
			}
		}

		SpawnWaspMutantVenomSackEffects(item, itemNumber);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
