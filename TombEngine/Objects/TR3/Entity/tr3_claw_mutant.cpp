#include "framework.h"
#include "Objects/TR3/Entity/tr3_claw_mutant.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Effects/enemy_missile.h"

using namespace TEN::Animation;
using namespace TEN::Entities::Effects;
using namespace TEN::Math;

// ItemFlags[5] flag enables damage left (0 = enabled, 1 = disabled).
// ItemFlags[6] flag enables damage right (0 = enabled, 1 = disabled).

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto CLAW_MUTANT_CLAW_ATTACK_DAMAGE	= 100;
	constexpr auto CLAW_MUTANT_PLASMA_ATTACK_DAMAGE = 200;

	constexpr auto CLAW_MUTANT_WALK_CHANCE = 1 / 64.0f;

	constexpr auto CLAW_MUTANT_IDLE_CLAW_ATTACK_RANGE	   = SQUARE(BLOCK(1.5f));
	constexpr auto CLAW_MUTANT_IDLE_DUAL_CLAW_ATTACK_RANGE = SQUARE(BLOCK(1.5f));
	constexpr auto CLAW_MUTANT_WALK_CLAW_ATTACK_RANGE	   = SQUARE(BLOCK(1));
	constexpr auto CLAW_MUTANT_RUN_CLAW_ATTACK_RANGE	   = SQUARE(BLOCK(2));
	constexpr auto CLAW_MUTANT_PLASMA_ATTACK_RANGE		   = SQUARE(BLOCK(3));

	constexpr auto CLAW_MUTANT_PLASMA_VELOCITY = 250;

	constexpr auto CLAW_MUTANT_WALK_TURN_RATE_MAX = ANGLE(3.0f);
	constexpr auto CLAW_MUTANT_RUN_TURN_RATE_MAX  = ANGLE(4.0f);

	const auto ClawMutantLeftBite  = CreatureBiteInfo(Vector3(19, -13, 3), 7);
	const auto ClawMutantRightBite = CreatureBiteInfo(Vector3(19, -13, 3), 4);
	const auto ClawMutantTailBite  = CreatureBiteInfo(Vector3(-32, -16, -119), 13);

	enum ClawMutantState
	{
		CLAW_MUTANT_STATE_IDLE = 0,
		CLAW_MUTANT_STATE_WALK = 1,
		CLAW_MUTANT_STATE_RUN = 2,
		CLAW_MUTANT_STATE_RUN_CLAW_ATTACK = 3,
		CLAW_MUTANT_STATE_WALK_CLAW_ATTACK_LEFT = 4,
		CLAW_MUTANT_STATE_WALK_CLAW_ATTACK_RIGHT = 5,
		CLAW_MUTANT_STATE_IDLE_CLAW_ATTACK_LEFT = 6,
		CLAW_MUTANT_STATE_IDLE_CLAW_ATTACK_RIGHT = 7,
		CLAW_MUTANT_STATE_DEATH = 8,
		CLAW_MUTANT_STATE_IDLE_DUAL_CLAW_ATTACK = 9,
		CLAW_MUTANT_STATE_PLASMA_ATTACK = 10
	};

	enum ClawMutantAnim
	{
		CLAW_MUTANT_ANIM_IDLE = 0,
		CLAW_MUTANT_ANIM_IDLE_TO_WALK_LEFT = 1,
		CLAW_MUTANT_ANIM_IDLE_TO_RUN_LEFT = 2,
		CLAW_MUTANT_ANIM_WALK_TO_IDLE_RIGHT = 3,
		CLAW_MUTANT_ANIM_WALK_TO_IDLE_LEFT = 4,
		CLAW_MUTANT_ANIM_RUN_TO_IDLE_RIGHT = 5,
		CLAW_MUTANT_ANIM_RUN_TO_IDLE_LEFT = 6,
		CLAW_MUTANT_ANIM_WALK = 7,
		CLAW_MUTANT_ANIM_WALK_TO_RUN = 8,
		CLAW_MUTANT_ANIM_RUN = 9,
		CLAW_MUTANT_ANIM_RUN_TO_WALK_LEFT = 10,
		CLAW_MUTANT_ANIM_RUN_TO_WALK_RIGHT = 11,
		CLAW_MUTANT_ANIM_RUN_CLAW_ATTACK = 12,
		CLAW_MUTANT_ANIM_RUN_CLAW_ATTACK_CANCEL = 13,
		CLAW_MUTANT_ANIM_IDLE_CLAW_ATTACK_LEFT = 14,
		CLAW_MUTANT_ANIM_IDLE_CLAW_ATTACK_RIGHT = 15,
		CLAW_MUTANT_ANIM_IDLE_DUAL_CLAW_ATTACK = 16,
		CLAW_MUTANT_ANIM_WALK_CLAW_ATTACK_LEFT = 17,
		CLAW_MUTANT_ANIM_WALK_CLAW_ATTACK_RIGHT = 18,
		CLAW_MUTANT_ANIM_PLASMA_ATTACK = 19,
		CLAW_MUTANT_ANIM_DEATH = 20
	};

	static void SpawnClawMutantPlasma(int itemNumber)
	{
		auto& plasma = *GetFreeParticle();

		plasma.on = true;
		plasma.sB = 255;
		plasma.sG = 48 + (GetRandomControl() & 31);
		plasma.sR = 48;

		plasma.dB = 192 + (GetRandomControl() & 63);
		plasma.dG = 128 + (GetRandomControl() & 63);
		plasma.dR = 32;

		plasma.colFadeSpeed = 12 + (GetRandomControl() & 3);
		plasma.fadeToBlack = 8;
		plasma.sLife =
		plasma.life = (GetRandomControl() & 7) + 24;

		plasma.blendMode = BlendMode::Additive;

		plasma.extras = 0;
		plasma.dynamic = -1;

		plasma.x = ((GetRandomControl() & 15) - 8);
		plasma.y = 0;
		plasma.z = ((GetRandomControl() & 15) - 8);

		plasma.xVel = ((GetRandomControl() & 31) - 16);
		plasma.yVel = (GetRandomControl() & 15) + 16;
		plasma.zVel = ((GetRandomControl() & 31) - 16);
		plasma.friction = 3;

		if (Random::TestProbability(1 / 2.0f))
		{
			plasma.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_ITEM | SP_NODEATTACH;
			plasma.rotAng = GetRandomControl() & 4095;

			if (Random::TestProbability(1 / 2.0f))
			{
				plasma.rotAdd = -(GetRandomControl() & 15) - 16;
			}
			else
			{
				plasma.rotAdd = (GetRandomControl() & 15) + 16;
			}
		}
		else
		{
			plasma.flags = SP_SCALE | SP_DEF | SP_EXPDEF | SP_ITEM | SP_NODEATTACH;
		}

		plasma.gravity = (GetRandomControl() & 31) + 16;
		plasma.maxYvel = (GetRandomControl() & 7) + 16;

		plasma.fxObj = itemNumber;
		plasma.nodeNumber = ParticleNodeOffsetIDs::NodeClawMutantPlasma;

		plasma.SpriteSeqID = ID_DEFAULT_SPRITES;
		plasma.SpriteID = 0;
		plasma.scalar = 1;
		int size = (GetRandomControl() & 31) + 64;
		plasma.size =
		plasma.sSize = size;
		plasma.dSize = size / 4;
	}

	static void SpawnClawMutantPlasmaBall(ItemInfo& item)
	{
		const auto& creature = *GetCreatureInfo(&item);

		int plasmaBall = CreateNewEffect(item.RoomNumber);
		if (plasmaBall == NO_VALUE)
			return;

		auto enemyPos = creature.Enemy->Pose.Position;
		if (creature.Enemy->IsLara() && GetLaraInfo(creature.Enemy)->Control.IsLow)
		{
			enemyPos.y -= CLICK(1);
		}
		else
		{
			enemyPos.y -= CLICK(2);
		}

		auto& fx = EffectList[plasmaBall];

		auto jointPos = GetJointPosition(item, ClawMutantTailBite.BoneID, ClawMutantTailBite.Position);
		auto orient = Geometry::GetOrientToPoint(jointPos.ToVector3(), enemyPos.ToVector3());

		fx.pos.Position = jointPos;
		fx.pos.Orientation.x = orient.x;
		fx.pos.Orientation.y = orient.y;
		fx.objectNumber = ID_ENERGY_BUBBLES;
		fx.color = Vector4::Zero;
		fx.speed = CLAW_MUTANT_PLASMA_VELOCITY;
		fx.flag2 = CLAW_MUTANT_PLASMA_ATTACK_DAMAGE;
		fx.flag1 = (int)MissileType::ClawMutantPlasma;
		fx.fallspeed = 0;
	}

	static void SpawnMutantPlasmaLight(ItemInfo& item)
	{
		int bright = item.Animation.FrameNumber;
		if (bright > 16)
		{
			bright = 28 + 16 - item.Animation.FrameNumber;
			if (bright > 16)
				bright = 16;
		}

		if (bright > 0)
		{
			auto pos = GetJointPosition(item, 13, Vector3i(-32, -16, -192));
			int rnd = GetRandomControl();
			byte r, g, b;

			b = 31 - ((rnd / 16) & 3);
			g = 24 - ((rnd / 64) & 3);
			r = rnd & 7;

			r = (r * bright) / 16;
			g = (g * bright) / 16;
			b = (b * bright) / 16;
			SpawnDynamicLight(pos.x, pos.y, pos.z, bright, r, g, b);
		}
	}

	static void DamageTargetWithClaw(ItemInfo& source, ItemInfo& target)
	{
		if (source.ItemFlags[5] == 0 && source.TouchBits.Test(ClawMutantLeftBite.BoneID))
		{
			DoDamage(&target, CLAW_MUTANT_CLAW_ATTACK_DAMAGE / 2);
			CreatureEffect2(&source, ClawMutantLeftBite, 10, source.Pose.Orientation.y, DoBloodSplat);
			source.ItemFlags[5] = 1;
		}

		if (source.ItemFlags[6] == 0 && source.TouchBits.Test(ClawMutantRightBite.BoneID))
		{
			DoDamage(&target, CLAW_MUTANT_CLAW_ATTACK_DAMAGE / 2);
			CreatureEffect2(&source, ClawMutantRightBite, 10, source.Pose.Orientation.y, DoBloodSplat);
			source.ItemFlags[6] = 1;
		}
	}

	void SpawnClawMutantPlasmaFlameBall(int fxNumber, const Vector3& vel, const Vector3& offset, float life)
	{
		auto& plasma = *GetFreeParticle();

		plasma.on = true;
		plasma.sB = 255;
		plasma.sG = 48 + (GetRandomControl() & 31);
		plasma.sR = 48;

		plasma.dB = 192 + (GetRandomControl() & 63);
		plasma.dG = 128 + (GetRandomControl() & 63);
		plasma.dR = 32;

		plasma.colFadeSpeed = 12 + (GetRandomControl() & 3);
		plasma.fadeToBlack = 8;
		plasma.sLife =
		plasma.life = (GetRandomControl() & 7) + life;

		plasma.blendMode = BlendMode::Additive;

		plasma.extras = 0;
		plasma.dynamic = -1;

		plasma.x = offset.x + ((GetRandomControl() & 15) - 8);
		plasma.y = 0;
		plasma.z = offset.z + ((GetRandomControl() & 15) - 8);

		plasma.xVel = vel.x;
		plasma.yVel = vel.y;
		plasma.zVel = vel.z;
		plasma.friction = 5;

		if (Random::TestProbability(1 / 2.0f))
		{
			plasma.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_FX;
			plasma.rotAng = GetRandomControl() & 4095;

			if (Random::TestProbability(1 / 2.0f))
			{
				plasma.rotAdd = -(GetRandomControl() & 15) - 16;
			}
			else
			{
				plasma.rotAdd = (GetRandomControl() & 15) + 16;
			}
		}
		else
		{
			plasma.flags = SP_SCALE | SP_DEF | SP_EXPDEF | SP_FX;
		}

		plasma.fxObj = fxNumber;
		plasma.SpriteSeqID = ID_DEFAULT_SPRITES;
		plasma.SpriteID = 0;
		plasma.scalar = 1;
		plasma.gravity =
		plasma.maxYvel = 0;

		int size = (GetRandomControl() & 31) + 64;
		plasma.size =
		plasma.sSize = size;
		plasma.dSize /= 16;

		plasma.yVel = (GetRandomControl() & 511) - 256;
		plasma.xVel *= 2;
		plasma.zVel *= 2;
		plasma.scalar = 2;
		plasma.friction = 85;
		plasma.gravity = 22;
	}

	void ClawMutantControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != CLAW_MUTANT_STATE_DEATH)
				SetAnimation(item, CLAW_MUTANT_ANIM_DEATH);

			int endFrameNumber = GetAnimData(item, CLAW_MUTANT_ANIM_DEATH).EndFrameNumber;
			if (item.Animation.FrameNumber >= endFrameNumber)
				CreatureDie(itemNumber, true);
		}
		else
		{
			if (item.AIBits)
				GetAITarget(&creature);

			AI_INFO ai;
			CreatureAIInfo(&item, &ai);
			GetCreatureMood(&item, &ai, ai.zoneNumber == ai.enemyZone ? true : false);
			CreatureMood(&item, &ai, ai.zoneNumber == ai.enemyZone ? true : false);

			headingAngle = CreatureTurn(&item, creature.MaxTurn);
			bool canShoot = (Targetable(&item, &ai) &&
				((ai.distance > CLAW_MUTANT_PLASMA_ATTACK_RANGE && !item.ItemFlags[0]) || ai.zoneNumber != ai.enemyZone));

			switch (item.Animation.ActiveState)
			{
			case CLAW_MUTANT_STATE_IDLE:
				item.ItemFlags[5] = 0;
				item.ItemFlags[6] = 0;
				creature.MaxTurn = 0;

				if (item.AIBits & GUARD)
				{
					extraHeadRot.y = AIGuard(&creature);
					item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE;
					break;
				}
				else if (item.AIBits & PATROL1)
				{
					extraHeadRot.y = 0;
					item.Animation.TargetState = CLAW_MUTANT_STATE_WALK;
				}
				else if (creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_RUN;
				}
				else if (ai.bite && ai.distance < CLAW_MUTANT_IDLE_CLAW_ATTACK_RANGE)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;

					if (ai.angle < 0)
					{
						item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE_CLAW_ATTACK_LEFT;
					}
					else
					{
						item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE_CLAW_ATTACK_RIGHT;
					}
				}
				else if (ai.bite && ai.distance < CLAW_MUTANT_IDLE_DUAL_CLAW_ATTACK_RANGE)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
					item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE_DUAL_CLAW_ATTACK;
				}
				else if (canShoot)
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_PLASMA_ATTACK;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					if (Random::TestProbability(CLAW_MUTANT_WALK_CHANCE))
						item.Animation.TargetState = CLAW_MUTANT_STATE_WALK;
				}
				else if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_RUN;
				}

				break;

			case CLAW_MUTANT_STATE_WALK:
				item.ItemFlags[5] = 0;
				item.ItemFlags[6] = 0;
				creature.MaxTurn = CLAW_MUTANT_WALK_TURN_RATE_MAX;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (item.AIBits & PATROL1)
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_WALK;
					extraHeadRot.y = 0;
				}
				else if (ai.bite && ai.distance < CLAW_MUTANT_WALK_CLAW_ATTACK_RANGE)
				{
					if (ai.angle < 0)
					{
						item.Animation.TargetState = CLAW_MUTANT_STATE_WALK_CLAW_ATTACK_LEFT;
					}
					else
					{
						item.Animation.TargetState = CLAW_MUTANT_STATE_WALK_CLAW_ATTACK_RIGHT;
					}
				}
				else if (canShoot)
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Escape || creature.Mood == MoodType::Attack)
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_RUN;
				}

				break;

			case CLAW_MUTANT_STATE_RUN:
				item.ItemFlags[5] = 0;
				item.ItemFlags[6] = 0;
				creature.MaxTurn = CLAW_MUTANT_RUN_TURN_RATE_MAX;

				if (ai.ahead)
					extraHeadRot.y = ai.angle;

				if (item.AIBits & GUARD)
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Bored)
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE;
				}
				else if (creature.Flags != 0 && ai.ahead)
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE;
				}
				else if (ai.bite && ai.distance < CLAW_MUTANT_RUN_CLAW_ATTACK_RANGE)
				{
					if (creature.Enemy != nullptr && creature.Enemy->Animation.Velocity.z == 0.0f)
					{
						item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE;
					}
					else
					{
						item.Animation.TargetState = CLAW_MUTANT_STATE_RUN_CLAW_ATTACK;
					}
				}
				else if (canShoot)
				{
					item.Animation.TargetState = CLAW_MUTANT_STATE_IDLE;
				}

				break;

			case CLAW_MUTANT_STATE_WALK_CLAW_ATTACK_LEFT:
			case CLAW_MUTANT_STATE_WALK_CLAW_ATTACK_RIGHT:
			case CLAW_MUTANT_STATE_RUN_CLAW_ATTACK:
			case CLAW_MUTANT_STATE_IDLE_DUAL_CLAW_ATTACK:
			case CLAW_MUTANT_STATE_IDLE_CLAW_ATTACK_LEFT:
			case CLAW_MUTANT_STATE_IDLE_CLAW_ATTACK_RIGHT:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				DamageTargetWithClaw(item, *creature.Enemy);
				break;

			case CLAW_MUTANT_STATE_PLASMA_ATTACK:
				if (ai.ahead)
				{
					extraTorsoRot.x = ai.xAngle;
					extraTorsoRot.y = ai.angle;
				}

				if (item.Animation.FrameNumber == 0 && Random::TestProbability(1 / 4.0f) == 0)
					item.ItemFlags[0] = 1;

				if (item.Animation.FrameNumber < 28)
				{
					SpawnClawMutantPlasma(itemNumber);
				}
				else if (item.Animation.FrameNumber == 28)
				{
					SpawnClawMutantPlasmaBall(item);
				}

				SpawnMutantPlasmaLight(item);
				break;
			}
		}

		CreatureJoint(&item, 0, extraTorsoRot.x);
		CreatureJoint(&item, 1, extraTorsoRot.y);
		CreatureJoint(&item, 2, extraHeadRot.y);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
