#include "framework.h"
#include "Objects/TR1/Entity/WingedMutant.h"

#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/missile.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR1
{
	constexpr auto WINGED_MUTANT_IDLE_JUMP_ATTACK_DAMAGE = 150;
	constexpr auto WINGED_MUTANT_RUN_JUMP_ATTACK_DAMAGE	 = 100;
	constexpr auto WINGED_MUTANT_SWIPE_ATTACK_DAMAGE	 = 200;

	constexpr auto WINGED_MUTANT_WALK_RANGE				 = SQUARE(BLOCK(4.5f));
	constexpr auto WINGED_MUTANT_SWIPE_ATTACK_RANGE		 = SQUARE(BLOCK(0.3f));
	constexpr auto WINGED_MUTANT_RUN_JUMP_ATTACK_RANGE	 = SQUARE(BLOCK(0.65f));
	constexpr auto WINGED_MUTANT_IDLE_JUMP_ATTACK_RANGE	 = SQUARE(BLOCK(2.5f));
	constexpr auto WINGED_MUTANT_PROJECTILE_ATTACK_RANGE = SQUARE(BLOCK(3.0f));
	constexpr auto WINGED_MUTANT_POSE_RANGE				 = SQUARE(BLOCK(4.5f));

	constexpr auto WINGED_MUTANT_POSE_CHANCE   = 1 / 400.0f;
	constexpr auto WINGED_MUTANT_UNPOSE_CHANCE = 1 / 164.0f;

	constexpr auto WINGED_MUTANT_FLY_VELOCITY	= BLOCK(1 / 32.0f);
	constexpr auto WINGED_MUTANT_SHARD_VELOCITY = 250;
	constexpr auto WINGED_MUTANT_BOMB_VELOCITY	= 220;

	constexpr auto WINGED_MUTANT_WALK_TURN_RATE_MAX = ANGLE(2.0f);
	constexpr auto WINGED_MUTANT_RUN_TURN_RATE_MAX	= ANGLE(6.0f);

	const auto WingedMutantBiteLeftHand	 = CreatureBiteInfo(Vector3(0, 0, 0), 7);
	const auto WingedMutantBiteRightHand = CreatureBiteInfo(Vector3(0, 0, 0), 10);
	const auto WingedMutantRocketBite	 = CreatureBiteInfo(Vector3(0, 200, 20), 6);
	const auto WingedMutantShardBite	 = CreatureBiteInfo(Vector3(0, 200, 20), 9);
	const auto WingedMutantHeadJoints	 = std::vector<unsigned int>{ 3 };
	const auto WingedMutantHandsJoints	 = std::vector<unsigned int>{ 7, 10 };
	const auto WingedMutantWingsJoints	 = std::vector<unsigned int>{ 15, 16, 17, 18, 19, 20 };

	enum WingedMutantState
	{
		// No state 0.
		WMUTANT_STATE_IDLE = 1,
		WMUTANT_STATE_WALK_FORWARD = 2,
		WMUTANT_STATE_RUN_FORWARD = 3,
		WMUTANT_STATE_IDLE_JUMP_ATTACK = 4,
		WMUTANT_STATE_DEATH = 5,
		WMUTANT_STATE_POSE = 6,
		WMUTANT_STATE_RUN_JUMP_ATTACK = 7,
		WMUTANT_STATE_SWIPE_ATTACK = 8,
		WMUTANT_STATE_AIM_DART = 9,
		WMUTANT_STATE_AIM_BOMB = 10,
		WMUTANT_STATE_SHOOT = 11,
		WMUTANT_STATE_INACTIVE = 12,
		WMUTANT_STATE_FLY = 13,
	};

	enum WingedMutantAnim
	{
		WMUTANT_ANIM_INACTIVE = 0,
		WMUTANT_ANIM_INACTIVE_TO_IDLE = 1,
		WMUTANT_ANIM_IDLE = 2,
		WMUTANT_ANIM_IDLE_TO_RUN = 3,
		WMUTANT_ANIM_RUN_FORWARD = 4,
		WMUTANT_ANIM_IDLE_JUMP_ATTACK_START = 5,
		WMUTANT_ANIM_IDLE_JUMP_ATTACK_END = 6,
		WMUTANT_ANIM_IDLE_TO_POSE = 7,
		WMUTANT_ANIM_POSE = 8,
		WMUTANT_ANIM_POSE_TO_IDLE = 9,
		WMUTANT_ANIM_POSE_TO_WALK_FORWARD = 10,
		WMUTANT_ANIM_WALK_FORWARD = 11,
		WMUTANT_ANIM_WALK_FORWARD_TO_IDLE = 12,
		WMUTANT_ANIM_WALK_FORWARD_TO_POSE = 13,
		WMUTANT_ANIM_RUN_JUMP_ATTACK = 14,
		WMUTANT_ANIM_IDLE_TO_AIM_1 = 15,
		WMUTANT_ANIM_AIM_DART = 16,
		WMUTANT_ANIM_SHOOT_DART = 17,
		WMUTANT_ANIM_AIM_DART_TO_IDLE = 18,
		WMUTANT_ANIM_IDLE_TO_AIM_BOMB = 19,
		WMUTANT_ANIM_SHOOT_BOMB = 20,
		WMUTANT_ANIM_RUN_FORWARD_TO_IDLE = 21,
		WMUTANT_ANIM_AIM_BOMB_TO_IDLE = 22,
		WMUTANT_ANIM_IDLE_TO_FLY = 23,
		WMUTANT_ANIM_FLY = 24,
		WMUTANT_ANIM_FLY_TO_IDLE = 25,
		WMUTANT_ANIM_SWIPE_ATTACK = 26
	};

	enum WingedMutantPathFinding
	{
		WMUTANT_PATH_GROUND = 1,
		WMUTANT_PATH_AERIAL = 2
	};

	// NOTE: Originally, winged mutants did not have OCBs. -- TokyoSU 5/8/2022
	enum WingedMutantOcb
	{
		WMUTANT_OCB_START_AERIAL		= (1 << 0),
		WMUTANT_OCB_START_INACTIVE		= (1 << 1),
		WMUTANT_OCB_START_POSE			= (1 << 2),
		WMUTANT_OCB_NO_WINGS			= (1 << 3),
		WMUTANT_OCB_DISABLE_DART_WEAPON = (1 << 4),
		WMUTANT_OCB_DISABLE_BOMB_WEAPON = (1 << 5)
	};

	enum WingedMutantProjectileType
	{
		WMUTANT_PROJ_NONE,
		WMUTANT_PROJ_SHARD,
		WMUTANT_PROJ_BOMB
	};

	enum WingedMutantConfig
	{
		WMUTANT_CONF_CAN_FLY,
		WMUTANT_CONF_PATHFINDING_MODE,
		WMUTANT_CONF_PROJECTILE_MODE,
		WMUTANT_CONF_NO_WINGS,
		WMUTANT_CONF_DISABLE_DART_WEAPON,
		WMUTANT_CONF_DISABLE_BOMB_WEAPON
	};

	static void SwitchPathfinding(ItemInfo& item, WingedMutantPathFinding path)
	{
		auto& creature = *GetCreatureInfo(&item);

		switch (path)
		{
		case WMUTANT_PATH_GROUND:
			creature.LOT.Step = CLICK(1);
			creature.LOT.Drop = -CLICK(1);
			creature.LOT.Fly = NO_FLYING;
			creature.LOT.Zone = ZoneType::Basic;
			break;

		case WMUTANT_PATH_AERIAL:
			creature.LOT.Step = BLOCK(20);
			creature.LOT.Drop = -BLOCK(20);
			creature.LOT.Fly = WINGED_MUTANT_FLY_VELOCITY;
			creature.LOT.Zone = ZoneType::Flyer;
			break;
		}
	}

	static WingedMutantProjectileType CanTargetPlayer(ItemInfo& item, AI_INFO& ai)
	{
		if (Targetable(&item, &ai) &&
			(ai.zoneNumber != ai.enemyZone || ai.distance > WINGED_MUTANT_PROJECTILE_ATTACK_RANGE))
		{
			if ((ai.angle > 0 && ai.angle < ANGLE(45.0f)) &&
				item.TestFlagField(WMUTANT_CONF_DISABLE_DART_WEAPON, false) && Objects[GAME_OBJECT_ID::ID_PROJ_SHARD].loaded)
			{
				return WMUTANT_PROJ_SHARD;
			}
			else if ((ai.angle < 0 && ai.angle > ANGLE(-45.0f)) &&
				item.TestFlagField(WMUTANT_CONF_DISABLE_BOMB_WEAPON, false), Objects[GAME_OBJECT_ID::ID_PROJ_BOMB].loaded)
			{
				return WMUTANT_PROJ_BOMB;
			}
		}

		// Cannot be targeted.
		return WMUTANT_PROJ_NONE;
	}

	static void InitializeWingedMutantOCB(ItemInfo& item)
	{
		if (item.TestOcb(WMUTANT_OCB_START_AERIAL))
		{
			SwitchPathfinding(item, WMUTANT_PATH_AERIAL);
			SetAnimation(item, WMUTANT_ANIM_FLY);
			item.SetFlagField(WMUTANT_CONF_PATHFINDING_MODE, WMUTANT_PATH_AERIAL);
		}
		else if (item.TestOcb(WMUTANT_OCB_START_INACTIVE))
		{
			SwitchPathfinding(item, WMUTANT_PATH_GROUND);
			SetAnimation(item, WMUTANT_ANIM_INACTIVE);
			item.SetFlagField(WMUTANT_CONF_PATHFINDING_MODE, WMUTANT_PATH_GROUND);
		}
		else if (item.TestOcb(WMUTANT_OCB_START_POSE))
		{
			SwitchPathfinding(item, WMUTANT_PATH_GROUND);
			SetAnimation(item, WMUTANT_ANIM_INACTIVE);
			item.SetFlagField(WMUTANT_CONF_PATHFINDING_MODE, WMUTANT_PATH_GROUND);
		}

		// Remove unnecessary OCBs.
		if (item.TestOcb(WMUTANT_OCB_START_AERIAL))
			item.RemoveOcb(WMUTANT_OCB_START_AERIAL);

		if (item.TestOcb(WMUTANT_OCB_START_INACTIVE))
			item.RemoveOcb(WMUTANT_OCB_START_INACTIVE);

		if (item.TestOcb(WMUTANT_OCB_START_POSE))
			item.RemoveOcb(WMUTANT_OCB_START_POSE);
	}

	void InitializeWingedMutant(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		item.SetFlagField(WMUTANT_CONF_PATHFINDING_MODE, WMUTANT_PATH_GROUND);
		item.SetFlagField(WMUTANT_CONF_PROJECTILE_MODE, WMUTANT_PROJ_NONE);

		if (item.TestOcb(WMUTANT_OCB_NO_WINGS))
		{
			item.SetFlagField(WMUTANT_CONF_CAN_FLY, false);
			item.MeshBits.Clear(WingedMutantWingsJoints);
		}
		else
		{
			item.SetFlagField(WMUTANT_CONF_CAN_FLY, true);
		}

		if (item.TestOcb(WMUTANT_OCB_DISABLE_BOMB_WEAPON))
			item.SetFlagField(WMUTANT_CONF_DISABLE_BOMB_WEAPON, true);

		if (item.TestOcb(WMUTANT_OCB_DISABLE_DART_WEAPON))
			item.SetFlagField(WMUTANT_CONF_DISABLE_DART_WEAPON, true);

		if (item.TestOcb(WMUTANT_OCB_DISABLE_BOMB_WEAPON))
			item.RemoveOcb(WMUTANT_OCB_DISABLE_BOMB_WEAPON);

		if (item.TestOcb(WMUTANT_OCB_DISABLE_DART_WEAPON))
			item.RemoveOcb(WMUTANT_OCB_DISABLE_DART_WEAPON);

		if (item.TestOcb(WMUTANT_OCB_NO_WINGS))
			item.RemoveOcb(WMUTANT_OCB_NO_WINGS);
	}

	void ControlWingedMutant(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);

		short headingAngle = 0;
		short headYOrient = 0;
		short torsoYOrient = 0; // Only when shooting.

		bool enableFlying = item.TestFlagField(WMUTANT_CONF_CAN_FLY, true);
		bool isFlying = item.TestFlagField(WMUTANT_CONF_PATHFINDING_MODE, WMUTANT_PATH_AERIAL);

		InitializeWingedMutantOCB(item);

		if (item.HitPoints <= 0)
		{
			CreatureDie(itemNumber, true, BODY_DO_EXPLOSION | BODY_PART_EXPLODE | BODY_NO_SMOKE | BODY_NO_SHATTER_EFFECT);
			
			auto pos = item.Pose;
			pos.Position.y -= CLICK(3);
			TriggerExplosionSparks(pos.Position.x, pos.Position.y, pos.Position.z, 3, -2, 0, item.RoomNumber);
			TriggerExplosionSparks(pos.Position.x, pos.Position.y, pos.Position.z, 3, -1, 0, item.RoomNumber);
			TriggerShockwave(&pos, 48, 304, (GetRandomControl() & 0x1F) + 112, 128, 32, 32, 32, EulerAngles(2048, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
			
			SoundEffect(SFX_TR1_ATLANTEAN_EXPLODE, &item.Pose);
			return;
		}
		else
		{
			AI_INFO ai;
			SwitchPathfinding(item, WMUTANT_PATH_GROUND);
			CreatureAIInfo(&item, &ai);

			bool isSameZoneInGroundMode = (ai.zoneNumber == ai.enemyZone);
			auto projectileType = CanTargetPlayer(item, ai);

			if (enableFlying && item.Animation.ActiveState == WMUTANT_STATE_FLY)
			{
				SwitchPathfinding(item, WMUTANT_PATH_AERIAL);
				CreatureAIInfo(&item, &ai);
			}

			if (ai.ahead)
			{
				headYOrient = ai.angle;
			}
			else
			{
				headYOrient = 0;
				torsoYOrient = 0;
			}

			GetCreatureMood(&item, &ai, isFlying);
			CreatureMood(&item, &ai, isFlying);
			headingAngle = CreatureTurn(&item, creature.MaxTurn);

			switch (item.Animation.ActiveState)
			{
			case WMUTANT_STATE_INACTIVE:
				creature.MaxTurn = 0;
				creature.Flags = 0;

				if (TargetVisible(&item, &ai) || creature.HurtByLara)
					item.Animation.TargetState = WMUTANT_STATE_IDLE;

				break;

			case WMUTANT_STATE_IDLE:
				item.SetFlagField(WMUTANT_CONF_PATHFINDING_MODE, WMUTANT_PROJ_NONE);
				creature.MaxTurn = 0;
				creature.Flags = 0;
				torsoYOrient = 0;

				if (enableFlying && !isSameZoneInGroundMode)
				{
					item.Animation.TargetState = WMUTANT_STATE_FLY;
					item.SetFlagField(WMUTANT_CONF_PATHFINDING_MODE, WMUTANT_PATH_AERIAL);
				}
				else if (item.TouchBits.Test(WingedMutantHeadJoints))
				{
					item.Animation.TargetState = WMUTANT_STATE_SWIPE_ATTACK;
				}
				else if (ai.bite && ai.distance < WINGED_MUTANT_IDLE_JUMP_ATTACK_RANGE)
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE_JUMP_ATTACK;
				}
				else if (ai.bite && ai.distance < WINGED_MUTANT_SWIPE_ATTACK_RANGE)
				{
					item.Animation.TargetState = WMUTANT_STATE_SWIPE_ATTACK;
				}
				else if (projectileType == WMUTANT_PROJ_SHARD)
				{
					item.Animation.TargetState = WMUTANT_STATE_AIM_DART;
				}
				else if (projectileType == WMUTANT_PROJ_BOMB)
				{
					item.Animation.TargetState = WMUTANT_STATE_AIM_BOMB;
				}
				else if (creature.Mood == MoodType::Bored ||
					(creature.Mood == MoodType::Stalk && ai.distance < WINGED_MUTANT_POSE_RANGE))
				{
					item.Animation.TargetState = WMUTANT_STATE_POSE;
				}
				else
				{
					item.Animation.TargetState = WMUTANT_STATE_RUN_FORWARD;
				}

				break;

			case WMUTANT_STATE_POSE:
				creature.Flags = 0;
				creature.MaxTurn = 0;
				headYOrient = 0; // NOTE: Pose has animation for head.

				if (projectileType != WMUTANT_PROJ_NONE || (isFlying && enableFlying))
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Stalk)
				{
					if (ai.distance < WINGED_MUTANT_WALK_RANGE)
					{
						if (isSameZoneInGroundMode ||
							Random::TestProbability(WINGED_MUTANT_UNPOSE_CHANCE))
						{
							item.Animation.TargetState = WMUTANT_STATE_WALK_FORWARD;
						}
					}
					else
					{
						item.Animation.TargetState = WMUTANT_STATE_IDLE;
					}
				}
				else if (creature.Mood == MoodType::Bored && Random::TestProbability(WINGED_MUTANT_UNPOSE_CHANCE))
				{
					item.Animation.TargetState = WMUTANT_STATE_WALK_FORWARD;
				}
				else if (creature.Mood == MoodType::Attack ||
					creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}

				break;

			case WMUTANT_STATE_WALK_FORWARD:
				creature.MaxTurn = WINGED_MUTANT_WALK_TURN_RATE_MAX;
				creature.Flags = 0;

				if (projectileType != WMUTANT_PROJ_NONE || (isFlying && enableFlying))
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Attack || creature.Mood == MoodType::Escape)
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}
				else if (creature.Mood == MoodType::Bored ||
					(creature.Mood == MoodType::Stalk && !isSameZoneInGroundMode))
				{
					if (Random::TestProbability(WINGED_MUTANT_POSE_CHANCE))
						item.Animation.TargetState = WMUTANT_STATE_POSE;
				}
				else if (creature.Mood == MoodType::Stalk &&
					ai.distance > WINGED_MUTANT_WALK_RANGE)
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}

				break;

			case WMUTANT_STATE_RUN_FORWARD:
				creature.MaxTurn = WINGED_MUTANT_RUN_TURN_RATE_MAX;
				creature.Flags = 0;

				if (enableFlying && !isSameZoneInGroundMode)
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}
				else if (projectileType != WMUTANT_PROJ_NONE)
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}
				else if (item.TouchBits.Test(WingedMutantHeadJoints))
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}
				else if (ai.bite && ai.distance < WINGED_MUTANT_RUN_JUMP_ATTACK_RANGE)
				{
					item.Animation.TargetState = WMUTANT_STATE_RUN_JUMP_ATTACK;
				}
				else if (ai.bite && ai.distance < WINGED_MUTANT_SWIPE_ATTACK_RANGE)
				{
					item.Animation.TargetState = WMUTANT_STATE_SWIPE_ATTACK;
				}
				else if (ai.ahead && ai.distance < WINGED_MUTANT_SWIPE_ATTACK_RANGE)
				{
					item.Animation.TargetState = WMUTANT_STATE_SWIPE_ATTACK;
				}
				else if (creature.Mood == MoodType::Bored ||
					(creature.Mood == MoodType::Stalk && ai.distance < WINGED_MUTANT_POSE_RANGE))
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}

				break;

			case WMUTANT_STATE_IDLE_JUMP_ATTACK:
				if (item.Animation.RequiredState == NO_VALUE &&
					(item.TouchBits.Test(WingedMutantHandsJoints) || item.TouchBits.Test(WingedMutantHeadJoints)) && creature.Flags == 0)
				{
					DoDamage(creature.Enemy, WINGED_MUTANT_IDLE_JUMP_ATTACK_DAMAGE / 2);
					CreatureEffect(&item, WingedMutantBiteLeftHand, DoBloodSplat);

					DoDamage(creature.Enemy, WINGED_MUTANT_IDLE_JUMP_ATTACK_DAMAGE / 2);
					CreatureEffect(&item, WingedMutantBiteRightHand, DoBloodSplat);

					item.Animation.TargetState = WMUTANT_STATE_IDLE;
					creature.Flags = 1;
				}

				break;

			case WMUTANT_STATE_RUN_JUMP_ATTACK:
				if (item.Animation.RequiredState == NO_VALUE &&
					(item.TouchBits.Test(WingedMutantHandsJoints) || item.TouchBits.Test(WingedMutantHeadJoints)) && creature.Flags == 0)
				{
					DoDamage(creature.Enemy, WINGED_MUTANT_RUN_JUMP_ATTACK_DAMAGE / 2);
					CreatureEffect(&item, WingedMutantBiteLeftHand, DoBloodSplat);

					DoDamage(creature.Enemy, WINGED_MUTANT_RUN_JUMP_ATTACK_DAMAGE / 2);
					CreatureEffect(&item, WingedMutantBiteRightHand, DoBloodSplat);

					item.Animation.TargetState = WMUTANT_STATE_RUN_FORWARD;
					creature.Flags = 1;
				}

				break;

			case WMUTANT_STATE_SWIPE_ATTACK:
				if (item.Animation.RequiredState == NO_VALUE &&
					item.TouchBits.Test(WingedMutantHandsJoints) && creature.Flags == 0)
				{
					DoDamage(creature.Enemy, WINGED_MUTANT_SWIPE_ATTACK_DAMAGE / 2);
					CreatureEffect(&item, WingedMutantBiteLeftHand, DoBloodSplat);

					DoDamage(creature.Enemy, WINGED_MUTANT_SWIPE_ATTACK_DAMAGE / 2);
					CreatureEffect(&item, WingedMutantBiteRightHand, DoBloodSplat);

					item.Animation.TargetState = WMUTANT_STATE_IDLE;
					creature.Flags = 1;
				}

				break;

			case WMUTANT_STATE_AIM_DART:
				item.SetFlagField(WMUTANT_CONF_PROJECTILE_MODE, WMUTANT_PROJ_SHARD);
				creature.MaxTurn = 0;
				creature.Flags = 0;
				torsoYOrient = ai.angle / 2;

				if (projectileType == WMUTANT_PROJ_SHARD)
				{
					item.Animation.TargetState = WMUTANT_STATE_SHOOT;
				}
				else
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}

				break;

			case WMUTANT_STATE_AIM_BOMB:
				item.SetFlagField(WMUTANT_CONF_PROJECTILE_MODE, WMUTANT_PROJ_BOMB);
				creature.MaxTurn = 0;
				creature.Flags = 0;
				torsoYOrient = ai.angle / 2;

				if (projectileType == WMUTANT_PROJ_BOMB)
				{
					item.Animation.TargetState = WMUTANT_STATE_SHOOT;
				}
				else
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE;
				}

				break;

			case WMUTANT_STATE_SHOOT:
				creature.MaxTurn = 0;
				torsoYOrient = ai.angle / 2;

				if (creature.Flags == 0)
				{
					if (projectileType == WMUTANT_PROJ_SHARD)
					{
						CreatureEffect2(&item, WingedMutantShardBite, WINGED_MUTANT_SHARD_VELOCITY, torsoYOrient, ShardGun);
					}
					else if (projectileType == WMUTANT_PROJ_BOMB)
					{
						CreatureEffect2(&item, WingedMutantRocketBite, WINGED_MUTANT_BOMB_VELOCITY, torsoYOrient, BombGun);
					}

					creature.Flags = 1;
				}
				
				item.SetFlagField(WMUTANT_CONF_PROJECTILE_MODE, WMUTANT_PROJ_NONE);
				break;

			case WMUTANT_STATE_FLY:
				if (creature.Mood != MoodType::Escape && isSameZoneInGroundMode)
				{
					item.Animation.TargetState = WMUTANT_STATE_IDLE; // Switch to ground mode.
					item.Pose.Position.y = item.Floor;
					item.SetFlagField(WMUTANT_CONF_PATHFINDING_MODE, WMUTANT_PATH_GROUND);
				}

				break;
			}
		}

		CreatureJoint(&item, 0, torsoYOrient);
		CreatureJoint(&item, 1, headYOrient);
		CreatureAnimation(itemNumber, headingAngle, 0);
	}
}
