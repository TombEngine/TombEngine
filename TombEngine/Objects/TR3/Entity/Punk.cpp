#include "framework.h"
#include "Objects/TR3/Entity/Punk.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Math;
using namespace TEN::Effects::Items;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto PUNK_HIT_DAMAGE   = 80;
	constexpr auto PUNK_SWIPE_DAMAGE = 100;

	constexpr auto PUNK_ATTACK0_RANGE = SQUARE(BLOCK(0.5f));
	constexpr auto PUNK_ATTACK1_RANGE = SQUARE(BLOCK(1));
	constexpr auto PUNK_ATTACK2_RANGE = SQUARE(BLOCK(1.25f));
	constexpr auto PUNK_WALK_RANGE    = SQUARE(BLOCK(1));
	constexpr auto PUNK_AWARE_DISTANCE = SQUARE(BLOCK(1));

	constexpr auto PUNK_WALK_TURN_RATE_MAX = ANGLE(5.0f);
	constexpr auto PUNK_RUN_TURN_RATE_MAX  = ANGLE(6.0f);

	constexpr auto PUNK_DIE_ANIM    = 26;
	constexpr auto PUNK_STOP_ANIM   = 6;
	constexpr auto PUNK_CLIMB1_ANIM = 28;
	constexpr auto PUNK_CLIMB2_ANIM = 29;
	constexpr auto PUNK_CLIMB3_ANIM = 27;
	constexpr auto PUNK_FALL3_ANIM  = 30;

	constexpr auto PUNK_VAULT_SHIFT = 260;
	constexpr auto PUNK_TOUCH = 0x2400;

	const auto PunkHitBite = CreatureBiteInfo(Vector3(16, 48, 320), 13);
	const auto PunkAttackJoints = std::vector<unsigned int>{ 13 };

	enum PunkState
	{
		PUNK_STATE_EMPTY = 0,
		PUNK_STATE_STOP = 1,
		PUNK_STATE_WALK = 2,
		PUNK_STATE_PUNCH2 = 3,
		PUNK_STATE_AIM2 = 4,
		PUNK_STATE_WAIT = 5,
		PUNK_STATE_AIM1 = 6,
		PUNK_STATE_AIM0 = 7,
		PUNK_STATE_PUNCH1 = 8,
		PUNK_STATE_PUNCH0 = 9,
		PUNK_STATE_RUN = 10,
		PUNK_STATE_DEATH = 11,
		PUNK_STATE_CLIMB3 = 12,
		PUNK_STATE_CLIMB1 = 13,
		PUNK_STATE_CLIMB2 = 14,
		PUNK_STATE_FALL3 = 15
	};

	// ItemFlags[2]: Flame counter (0 = no flame, 1 = permanent flame that sets Lara on fire, >1 = temporary flame)
	static void TriggerPunkFlame(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Distance check.
		int dx = LaraItem->Pose.Position.x - item.Pose.Position.x;
		int dz = LaraItem->Pose.Position.z - item.Pose.Position.z;
		if (dx < -BLOCK(16) || dx > BLOCK(16) || dz < -BLOCK(16) || dz > BLOCK(16))
			return;

		auto& spark = *GetFreeParticle();

		spark.on = true;
		spark.sR = 255;
		spark.sG = 48 + (GetRandomControl() & 31);
		spark.sB = 48;

		spark.dR = 192 + (GetRandomControl() & 63);
		spark.dG = 128 + (GetRandomControl() & 63);
		spark.dB = 32;

		spark.colFadeSpeed = 12 + (GetRandomControl() & 3);
		spark.fadeToBlack = 8;
		spark.sLife = spark.life = (GetRandomControl() & 7) + 24;

		spark.blendMode = BlendMode::Additive;
		spark.extras = 0;
		spark.dynamic = -1;

		spark.x = (GetRandomControl() & 15) - 8;
		spark.y = 0;
		spark.z = (GetRandomControl() & 15) - 8;

		spark.xVel = (GetRandomControl() & 255) - 128;
		spark.yVel = -(GetRandomControl() & 15) - 16;
		spark.zVel = (GetRandomControl() & 255) - 128;
		spark.friction = 5;

		if (GetRandomControl() & 1)
		{
			spark.gravity = -(GetRandomControl() & 31) - 16;
			spark.maxYvel = -(GetRandomControl() & 7) - 16;
			spark.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_ITEM | SP_NODEATTACH;
			spark.rotAng = GetRandomControl() & 4095;
			spark.rotAdd = (GetRandomControl() & 1) ? -((GetRandomControl() & 15) + 16) : ((GetRandomControl() & 15) + 16);
		}
		else
		{
			spark.flags = SP_SCALE | SP_DEF | SP_EXPDEF | SP_ITEM | SP_NODEATTACH;
			spark.gravity = -(GetRandomControl() & 31) - 16;
			spark.maxYvel = -(GetRandomControl() & 7) - 16;
		}

		spark.fxObj = itemNumber;
		spark.nodeNumber = NodePunkFlame;

		spark.SpriteSeqID = ID_DEFAULT_SPRITES;
		spark.SpriteID = 0;
		spark.scalar = 1;

		int size = (GetRandomControl() & 31) + 64;
		spark.size = spark.sSize = size;
		spark.dSize = size / 4;
	}

	static void SpawnPunkFlameLight(ItemInfo& item)
	{
		int rnd = GetRandomControl();
		auto pos = GetJointPosition(item, PunkHitBite.BoneID, Vector3i(
			PunkHitBite.Position.x + (rnd & 15) - 8,
			PunkHitBite.Position.y + ((rnd >> 4) & 15) - 8,
			PunkHitBite.Position.z + ((rnd >> 8) & 15) - 8));

		SpawnDynamicLight(pos.x, pos.y, pos.z, 13,
			31 - ((rnd >> 4) & 3),
			24 - ((rnd >> 6) & 3),
			rnd & 7);
	}

	void InitializePunk(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, PUNK_STOP_ANIM);

		item.ItemFlags[2] = item.TriggerFlags;
	}

	void ControlPunk(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short head = 0;
		auto extraTorsoRot = EulerAngles::Identity;

		// Spawn flame effect if punk has flaming torch.
		if (item->ItemFlags[2])
		{
			TriggerPunkFlame(itemNumber);
			SpawnPunkFlameLight(*item);
		}

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != PUNK_STATE_DEATH)
			{
				SetAnimation(*item, PUNK_DIE_ANIM);
				creature->LOT.Step = CLICK(1);
			}
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else
				creature->Enemy = LaraItem;

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

			if (!creature->Alerted && creature->Enemy == LaraItem)
				creature->Enemy = nullptr;

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			angle = CreatureTurn(item, creature->MaxTurn);

			auto* realEnemy = creature->Enemy;
			creature->Enemy = LaraItem;

			// Alert only if hit, OR (can see Lara AND not following).
			// Friendly punk with FOLLOW bit won't alert unless attacked.
			if (item->HitStatus ||
				((laraAI.distance < PUNK_AWARE_DISTANCE || TargetVisible(item, &laraAI)) &&
				 abs(LaraItem->Pose.Position.y - item->Pose.Position.y) < CLICK(5) &&
				 !(item->AIBits & FOLLOW)))
			{
				if (!creature->Alerted)
					SoundEffect(SFX_TR3_AMERCAN_HOY, &item->Pose);

				AlertAllGuards(itemNumber);
			}

			creature->Enemy = realEnemy;

			switch (item->Animation.ActiveState)
			{
			case PUNK_STATE_WAIT:
			case PUNK_STATE_STOP:
				// WAIT bails out early once alerted or already heading into a run.
				if (item->Animation.ActiveState == PUNK_STATE_WAIT &&
					(creature->Alerted || item->Animation.TargetState == PUNK_STATE_RUN))
				{
					item->Animation.TargetState = PUNK_STATE_STOP;
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
						if (item->Animation.ActiveState == PUNK_STATE_STOP)
							item->Animation.TargetState = PUNK_STATE_WAIT;
						else
							item->Animation.TargetState = PUNK_STATE_STOP;
					}
				}
				else if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = PUNK_STATE_WALK;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && ai.ahead && !item->HitStatus)
						item->Animation.TargetState = PUNK_STATE_STOP;
					else
						item->Animation.TargetState = PUNK_STATE_RUN;
				}
				else if (creature->Mood == MoodType::Bored ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					if (item->Animation.RequiredState != NO_VALUE)
						item->Animation.TargetState = item->Animation.RequiredState;
					else if (ai.ahead)
						item->Animation.TargetState = PUNK_STATE_STOP;
					else
						item->Animation.TargetState = PUNK_STATE_RUN;
				}
				else if (ai.bite && ai.distance < PUNK_ATTACK0_RANGE)
				{
					item->Animation.TargetState = PUNK_STATE_AIM0;
				}
				else if (ai.bite && ai.distance < PUNK_ATTACK1_RANGE)
				{
					item->Animation.TargetState = PUNK_STATE_AIM1;
				}
				else if (ai.bite && ai.distance < PUNK_WALK_RANGE)
				{
					item->Animation.TargetState = PUNK_STATE_WALK;
				}
				else
				{
					item->Animation.TargetState = PUNK_STATE_RUN;
				}

				break;

			case PUNK_STATE_WALK:
				head = laraAI.angle;
				creature->MaxTurn = PUNK_WALK_TURN_RATE_MAX;

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = PUNK_STATE_WALK;
					head = 0;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = PUNK_STATE_RUN;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (Random::TestProbability(1 / 256.0f))
					{
						item->Animation.RequiredState = PUNK_STATE_WAIT;
						item->Animation.TargetState = PUNK_STATE_STOP;
					}
				}
				else if (ai.bite && ai.distance < PUNK_ATTACK0_RANGE)
				{
					item->Animation.TargetState = PUNK_STATE_STOP;
				}
				else if (ai.bite && ai.distance < PUNK_ATTACK2_RANGE)
				{
					item->Animation.TargetState = PUNK_STATE_AIM2;
				}
				else
				{
					item->Animation.TargetState = PUNK_STATE_RUN;
				}

				break;

			case PUNK_STATE_RUN:
				if (ai.ahead)
					head = ai.angle;

				creature->MaxTurn = PUNK_RUN_TURN_RATE_MAX;
				tilt = angle / 2;

				if (item->AIBits & GUARD)
				{
					item->Animation.TargetState = PUNK_STATE_WAIT;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					if (Lara.TargetEntity != item && ai.ahead)
						item->Animation.TargetState = PUNK_STATE_STOP;
				}
				else if ((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2))))
				{
					item->Animation.TargetState = PUNK_STATE_STOP;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					item->Animation.TargetState = PUNK_STATE_WALK;
				}
				else if (ai.ahead && ai.distance < PUNK_WALK_RANGE)
				{
					item->Animation.TargetState = PUNK_STATE_WALK;
				}

				break;

			case PUNK_STATE_AIM0:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PUNK_WALK_TURN_RATE_MAX;
				creature->Flags = 0;

				if (ai.bite && ai.distance < PUNK_ATTACK0_RANGE)
					item->Animation.TargetState = PUNK_STATE_PUNCH0;
				else
					item->Animation.TargetState = PUNK_STATE_STOP;

				break;

			case PUNK_STATE_AIM1:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PUNK_WALK_TURN_RATE_MAX;
				creature->Flags = 0;

				if (ai.ahead && ai.distance < PUNK_ATTACK1_RANGE)
					item->Animation.TargetState = PUNK_STATE_PUNCH1;
				else
					item->Animation.TargetState = PUNK_STATE_STOP;

				break;

			case PUNK_STATE_AIM2:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PUNK_WALK_TURN_RATE_MAX;
				creature->Flags = 0;

				if (ai.bite && ai.distance < PUNK_ATTACK2_RANGE)
					item->Animation.TargetState = PUNK_STATE_PUNCH2;
				else
					item->Animation.TargetState = PUNK_STATE_WALK;

				break;

			case PUNK_STATE_PUNCH0:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PUNK_WALK_TURN_RATE_MAX;

				if (!creature->Flags && item->TouchBits.Test(PunkAttackJoints))
				{
					DoDamage(creature->Enemy, PUNK_HIT_DAMAGE);
					CreatureEffect(item, PunkHitBite, DoBloodSplat);
					SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);

					// Flaming torch logic.
					if (item->ItemFlags[2] == 1)
						ItemBurn(creature->Enemy);
					else if (item->ItemFlags[2] > 1)
						item->ItemFlags[2]--;

					creature->Flags = 1;
				}

				break;

			case PUNK_STATE_PUNCH1:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PUNK_WALK_TURN_RATE_MAX;

				if (!creature->Flags && item->TouchBits.Test(PunkAttackJoints))
				{
					DoDamage(creature->Enemy, PUNK_HIT_DAMAGE);
					CreatureEffect(item, PunkHitBite, DoBloodSplat);
					SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);

					// Flaming torch logic.
					if (item->ItemFlags[2] == 1)
						ItemBurn(creature->Enemy);
					else if (item->ItemFlags[2] > 1)
						item->ItemFlags[2]--;

					creature->Flags = 1;
				}

				if (ai.ahead && ai.distance > PUNK_ATTACK1_RANGE && ai.distance < PUNK_ATTACK2_RANGE)
					item->Animation.TargetState = PUNK_STATE_PUNCH2;

				break;

			case PUNK_STATE_PUNCH2:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				creature->MaxTurn = PUNK_WALK_TURN_RATE_MAX;

				if (creature->Flags != 2 && item->TouchBits.Test(PunkAttackJoints))
				{
					DoDamage(creature->Enemy, PUNK_SWIPE_DAMAGE);
					CreatureEffect(item, PunkHitBite, DoBloodSplat);
					SoundEffect(SFX_TR4_LARA_THUD, &item->Pose);

					// Flaming torch logic.
					if (item->ItemFlags[2] == 1)
						ItemBurn(creature->Enemy);
					else if (item->ItemFlags[2] > 1)
						item->ItemFlags[2]--;

					creature->Flags = 2;
				}

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, head);

		if (item->Animation.ActiveState < PUNK_STATE_DEATH)
		{
			switch (CreatureVault(itemNumber, angle, 2, PUNK_VAULT_SHIFT))
			{
			case 2:
				creature->MaxTurn = 0;
				SetAnimation(*item, PUNK_CLIMB1_ANIM);
				break;

			case 3:
				creature->MaxTurn = 0;
				SetAnimation(*item, PUNK_CLIMB2_ANIM);
				break;

			case 4:
				creature->MaxTurn = 0;
				SetAnimation(*item, PUNK_CLIMB3_ANIM);
				break;

			case -4:
				creature->MaxTurn = 0;
				SetAnimation(*item, PUNK_FALL3_ANIM);
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
