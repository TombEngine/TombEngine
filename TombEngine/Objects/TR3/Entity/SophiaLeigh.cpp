#include "framework.h"
#include "Objects/TR3/Entity/SophiaLeigh.h"

#include "Game/Animation/Animation.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Game/misc.h"
#include "Game/setup.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/effects/tomb4fx.h"
#include "Game/people.h"
#include "Game/effects/spark.h"
#include "Objects/Effects/Boss.h"
#include "Objects/Effects/enemy_missile.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Boss;
using namespace TEN::Entities::Effects;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto SOPHIALEIGH_WALK_RANGE		   = SQUARE(BLOCK(1));
	constexpr auto SOPHIALEIGH_NORMAL_ATTACK_RANGE = SQUARE(BLOCK(5));
	constexpr auto SOPHIALEIGH_NORMAL_WALK_RANGE   = SQUARE(BLOCK(5));
	constexpr auto SOPHIALEIGH_Y_DISTANCE_RANGE	   = BLOCK(1.5f);
	constexpr auto SOPHIALEIGH_REACHED_GOAL_RANGE  = BLOCK(0.5f);
	constexpr auto SOPHIALEIGH_KNOCKBACK_RANGE     = BLOCK(3);

	constexpr auto SOPHIALEIGH_DAMAGE_SMALL_BOLT = 4;
	constexpr auto SOPHIALEIGH_DAMAGE_LARGE_BOLT = 10;

	constexpr auto SOPHIALEIGH_CHARGE_TIMER_DURATION = 600;
	constexpr auto SOPHIALEIGH_EXPLOSION_NUM_MAX	 = 60;

	constexpr auto SOPHIALEIGH_EFFECT_COLOR			  = Vector4(0.0f, 0.7f, 0.3f, 0.5f);
	constexpr auto SOPHIALEIGH_SHOCKWAVE_COLOR		  = Vector4(0.0f, 0.7f, 0.3f, 0.5f);
	constexpr auto SOPHIALEIGH_EXPLOSION_MAIN_COLOR   = Vector4(0.0f, 0.7f, 0.2f, 0.5f);
	constexpr auto SOPHIALEIGH_EXPLOSION_SECOND_COLOR = Vector4(0.0f, 0.7f, 0.0f, 0.5f);

	constexpr auto SOPHIALEIGH_WALK_TURN_RATE_MAX					= ANGLE(4.0f);
	constexpr auto SOPHIALEIGH_RUN_TURN_RATE_MAX					= ANGLE(7.0f);
	constexpr auto SOPHIALEIGH_LASER_DECREASE_XANGLE_IF_LARA_CROUCH = ANGLE(0.25f);
	constexpr auto SOPHIALEIGH_LASER_DISPERSION_ANGLE				= ANGLE(1.5f);

	constexpr auto SOPHIALEIGH_LIGHTNING_GLOW_SIZE	   = 8;
	constexpr auto SOPHIALEIGH_MAX_LIGHTNING_GLOW_SIZE = 10;
	constexpr auto SOPHIALEIGH_SHOCKWAVE_SPEED		   = -184;
	constexpr auto SOPHIALEIGH_SHOCKWAVE_INNER_SIZE	   = 2700;
	constexpr auto SOPHIALEIGH_SHOCKWAVE_OUTER_SIZE	   = 2300;

	constexpr auto SOPHIALEIGH_KNOCKBACK_LARGE_INNER_SIZE = 800;
	constexpr auto SOPHIALEIGH_KNOCKBACK_LARGE_OUTER_SIZE = 0;
	constexpr auto SOPHIALEIGH_KNOCKBACK_SMALL_INNER_SIZE = 200;
	constexpr auto SOPHIALEIGH_KNOCKBACK_SMALL_OUTER_SIZE = -400;

	constexpr auto SOPHIALEIGH_VAULT_SHIFT = 96;

	const auto SophiaLeighStaffBite = CreatureBiteInfo(Vector3(-28, 56, 356), 10);
	const auto SophiaLeighLeftBite	= CreatureBiteInfo(Vector3(-72, 48, 356), 10);
	const auto SophiaLeighRightBite = CreatureBiteInfo(Vector3(16, 48, 304), 10);

	struct SophiaData
	{
		short angle;
		short tilt;
		short headAngle;
		short torsoXAngle;
		short torsoYAngle;
		short shockwaveCount;
		short shockwaveTimer;
	};

	enum SophiaLeighState
	{
		// No state 0.
		SOPHIALEIGH_STATE_STAND = 1,
		SOPHIALEIGH_STATE_WALK = 2,
		SOPHIALEIGH_STATE_RUN = 3,
		SOPHIALEIGH_STATE_SUMMON = 4,
		SOPHIALEIGH_STATE_BIG_SHOOT = 5,
		SOPHIALEIGH_STATE_DEATH = 6,
		SOPHIALEIGH_STATE_LAUGH = 7,
		SOPHIALEIGH_STATE_SMALL_SHOOT = 8,
		SOPHIALEIGH_STATE_CLIMB2 = 9,
		SOPHIALEIGH_STATE_CLIMB3 = 10,
		SOPHIALEIGH_STATE_CLIMB4 = 11,
		SOPHIALEIGH_STATE_FALL4CLICK = 12,
	};

	enum SophiaLeighAnim
	{
		SOPHIALEIGH_ANIM_WALK = 0,
		SOPHIALEIGH_ANIM_SUMMON_START = 1,
		SOPHIALEIGH_ANIM_SUMMON = 2,
		SOPHIALEIGH_ANIM_SUMMON_END = 3,
		SOPHIALEIGH_ANIM_SCEPTER_AIM = 4,
		SOPHIALEIGH_ANIM_SCEPTER_SHOOT = 5,
		SOPHIALEIGH_ANIM_SCEPTER_AIM_TO_IDLE = 6,
		SOPHIALEIGH_ANIM_IDLE = 7,
		SOPHIALEIGH_ANIM_LAUGH = 8,
		SOPHIALEIGH_ANIM_CLIMB2CLICK = 9,
		SOPHIALEIGH_ANIM_CLIMB2CLICK_END = 10,
		SOPHIALEIGH_ANIM_WALK_STOP = 11,
		SOPHIALEIGH_ANIM_RUN = 12,
		SOPHIALEIGH_ANIM_RUN_TO_STAND_LEFT = 13,
		SOPHIALEIGH_ANIM_RUN_TO_WALK_RIGHT = 14,
		SOPHIALEIGH_ANIM_CLIMB4CLICK = 15,
		SOPHIALEIGH_ANIM_WALK_START = 16,
		SOPHIALEIGH_ANIM_DEATH = 17,
		SOPHIALEIGH_ANIM_CLIMB3CLICK = 18,
		SOPHIALEIGH_ANIM_WALK_TO_RUN_RIGHT = 19,
		SOPHIALEIGH_ANIM_RUN_START = 20,
		SOPHIALEIGH_ANIM_FALL4CLICK = 21,
		SOPHIALEIGH_ANIM_WALK_STOP_LEFT = 22,
		SOPHIALEIGH_ANIM_RUN_TO_WALK_LEFT = 23,
		SOPHIALEIGH_ANIM_RUN_TO_STAND_RIGHT = 24,
		SOPHIALEIGH_ANIM_SCEPTER_SMALL_SHOOT = 25
	};

	enum class SophiaOCB
	{
		Normal = 0,			 // Move, climb, attack, and chase player.
		Tower = 1,			 // TR3 one, with climbing only.
		TowerWithVolume = 2, // TR3 one, but uses volume to move instead of height check. Must increase/decrease creature->LocationAI to go up/down.
	};

	static void RotateTowardTarget(ItemInfo& item, const AI_INFO& ai, short turnRate)
	{
		if (abs(ai.angle) < turnRate)
		{
			item.Pose.Orientation.y += ai.angle;
		}
		else if (ai.angle < 0)
		{
			item.Pose.Orientation.y -= turnRate;
		}
		else
		{
			item.Pose.Orientation.y += turnRate;
		}
	}

	static void KnockbackCollision(ItemInfo& item, short headingAngle)
	{
		DoDamage(&item, 200);
		item.HitStatus = true;

		short diff = item.Pose.Orientation.y - headingAngle;
		// Facing away from ring.
		if (abs(diff) < ANGLE(90.0f))
			item.Animation.Velocity.z = 75.0f;
		// Facing toward ring.
		else
			item.Animation.Velocity.z = -75.0f;

		item.Animation.IsAirborne = true;
		item.Animation.Velocity.y = -50.0f;
		item.Pose.Orientation.x = 0;
		item.Pose.Orientation.z = 0;
		SetAnimation(item, LA_FALL_BACK);
	}

	static void TriggerKnockback(ItemInfo& item, int life = 32)
	{
		auto& creature = *GetCreatureInfo(&item);
		auto& enemy = *creature.Enemy;

		auto orient = Geometry::GetOrientToPoint(enemy.Pose.Position.ToVector3(), item.Pose.Position.ToVector3());

		float distance = Vector3::Distance(item.Pose.Position.ToVector3(), enemy.Pose.Position.ToVector3());
		if (distance <= SOPHIALEIGH_KNOCKBACK_RANGE)
		{
			byte red = SOPHIALEIGH_EFFECT_COLOR.x * UCHAR_MAX;
			byte green = SOPHIALEIGH_EFFECT_COLOR.y * UCHAR_MAX;
			byte blue = SOPHIALEIGH_EFFECT_COLOR.z * UCHAR_MAX;

			auto sphere = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(2), 0.0f), BLOCK(1 / 16.0f));
			auto centerPos = Pose(Random::GeneratePointInSphere(sphere), item.Pose.Orientation);

			auto sphere1 = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(3), 0.0f), BLOCK(1 / 16.0f));
			auto upperPos = Pose(Random::GeneratePointInSphere(sphere1), item.Pose.Orientation);

			auto sphere2 = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(1), 0.0f), BLOCK(1 / 16.0f));
			auto lowerPos = Pose(Random::GeneratePointInSphere(sphere2), item.Pose.Orientation);

			// Upper position.
			TriggerShockwave(
				&upperPos, SOPHIALEIGH_KNOCKBACK_SMALL_INNER_SIZE, SOPHIALEIGH_KNOCKBACK_SMALL_OUTER_SIZE, 184,
				red, green, blue,
				36, EulerAngles(0, 30, 0), 0, false, true, false, (int)ShockwaveStyle::Knockback);

			// Center position.
			TriggerShockwave(
				&centerPos, SOPHIALEIGH_KNOCKBACK_LARGE_INNER_SIZE, SOPHIALEIGH_KNOCKBACK_LARGE_OUTER_SIZE, 184,
				red, green, blue,
				36, EulerAngles(0, 30, 0), 0, false, true, false, (int)ShockwaveStyle::Knockback);

			// Lower position.
			TriggerShockwave(
				&lowerPos, SOPHIALEIGH_KNOCKBACK_SMALL_INNER_SIZE, SOPHIALEIGH_KNOCKBACK_SMALL_OUTER_SIZE, 184,
				red, green, blue,
				36, EulerAngles(0, 30, 0), 0, false, true, false, (int)ShockwaveStyle::Knockback);

			TriggerExplosionSparks(enemy.Pose.Position.x, enemy.Pose.Position.y, enemy.Pose.Position.z, 3, -2, 2, enemy.RoomNumber);
			// NOTE: TriggerPlasmaBall exists but isn't coded (uses EXTRAFX5 in OG).

			KnockbackCollision(enemy, orient.y);
		}
	}

	static void TriggerSophiaLeightLight(ItemInfo& item, const Vector3& pos)
	{
		if ((item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SUMMON_START && item.Animation.FrameNumber > 6) ||
			 item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SUMMON ||
			(item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SUMMON_END && item.Animation.FrameNumber < 3) ||
			(item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SCEPTER_SHOOT && item.Animation.FrameNumber > 39 && item.Animation.FrameNumber < 47) ||
			(item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SCEPTER_SMALL_SHOOT && item.Animation.FrameNumber > 14 && item.Animation.FrameNumber < 18))
		{
			SpawnDynamicLight(
				pos.x, pos.y, pos.z,
				item.ItemFlags[1] + SOPHIALEIGH_LIGHTNING_GLOW_SIZE,
				SOPHIALEIGH_EFFECT_COLOR.x * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.y * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.z * UCHAR_MAX);

			if (item.ItemFlags[1] < SOPHIALEIGH_MAX_LIGHTNING_GLOW_SIZE)
				item.ItemFlags[1]++;
		}
		else if (item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SUMMON_END && item.Animation.FrameNumber >= 3 && item.ItemFlags[1] > 0)
		{
			SpawnDynamicLight(
				pos.x, pos.y, pos.z,
				item.ItemFlags[1] + SOPHIALEIGH_LIGHTNING_GLOW_SIZE,
				SOPHIALEIGH_EFFECT_COLOR.x * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.y * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.z * UCHAR_MAX);

			item.ItemFlags[1]--;
		}
	}

	static void SpawnSophiaLeighProjectileBolt(ItemInfo& item, ItemInfo* enemy, const CreatureBiteInfo& bite, SophiaData* data, bool isBoltLarge, short angleAdd)
	{
		int fxNumber = CreateNewEffect(item.RoomNumber);
		if (fxNumber == NO_VALUE)
			return;

		auto& fx = EffectList[fxNumber];

		auto boltType = isBoltLarge ? (short)MissileType::SophiaLeighLarge : (short)MissileType::SophiaLeighNormal;

		fx.pos.Position = GetJointPosition(&item, bite);
		fx.pos.Orientation.x = item.Pose.Orientation.x + data->torsoXAngle;

		if (enemy->IsLara())
		{
			const auto& player = *GetLaraInfo(enemy);
			if (player.Control.IsLow)
				fx.pos.Orientation.x -= SOPHIALEIGH_LASER_DECREASE_XANGLE_IF_LARA_CROUCH;
		}

		fx.pos.Orientation.y = (item.Pose.Orientation.y + data->torsoYAngle) + angleAdd;
		fx.pos.Orientation.z = 0;
		fx.roomNumber = item.RoomNumber;
		fx.counter = 0;
		fx.flag1 = boltType;
		fx.flag2 = isBoltLarge ? SOPHIALEIGH_DAMAGE_LARGE_BOLT : SOPHIALEIGH_DAMAGE_SMALL_BOLT; // Damage value.
		fx.speed = Random::GenerateInt(120, 160);
		fx.objectNumber = ID_ENERGY_BUBBLES;
		fx.frameNumber = Objects[fx.objectNumber].meshIndex + (boltType - 1);
	}

	// TR3 Behaviour, which let sophia go to AI_X1 object to move up/down a "tower"
	static void SophiaLeighTowerControl(ItemInfo& item, CreatureInfo* creature, SophiaData* data)
	{
		if (item.AIBits)
			GetAITarget(creature);

		auto sphere = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(2), 0.0f), BLOCK(1 / 16.0f));
		auto shockwavePos = Pose(Random::GeneratePointInSphere(sphere), item.Pose.Orientation);
		TriggerSophiaLeightLight(item, shockwavePos.Position.ToVector3());

		AI_INFO ai;
		CreatureAIInfo(&item, &ai);

		// Check the previous and next position of AI object to
		// allow Sophia to go up or down based on enemy's vertical position.
		FindAITargetObject(creature, ID_AI_X1, creature->LocationAI, false);

		if (Vector3i::Distance(item.Pose.Position, creature->Enemy->Pose.Position) < SOPHIALEIGH_REACHED_GOAL_RANGE)
		{
			creature->ReachedGoal = true;
			creature->Enemy = LaraItem; // TODO: Deal with LaraItem global.

			if (item.TriggerFlags == (int)SophiaOCB::Tower)
			{
				// If enemy is above, get to next AI_X1.
				if (ai.verticalDistance >= SOPHIALEIGH_Y_DISTANCE_RANGE)
					creature->LocationAI++;

				// If enemy is below, get to previous AI_X1.
				else if (ai.verticalDistance <= -SOPHIALEIGH_Y_DISTANCE_RANGE)
					creature->LocationAI--;
			}
		}
		else
		{
			creature->ReachedGoal = false;
		}

		// Charge count. Sophia can start the charge animation again when at 0.
		if (item.Timer > 0)
			item.Timer--;

		if (ai.ahead)
		{
			data->headAngle = ai.angle;
		}
		else
		{
			data->torsoXAngle = 0;
			data->torsoYAngle = 0;
		}

		GetCreatureMood(&item, &ai, true);
		CreatureMood(&item, &ai, true);

		// Knockback the target if Sophia in tower mode.
		// Avoid spawning rings if target is dead.
		if (ai.distance < SOPHIALEIGH_KNOCKBACK_RANGE && creature->Flags <= 0 && creature->Enemy->HitPoints > 0)
		{
			TriggerKnockback(item);
			creature->Flags = 50;
		}
		else
		{
			creature->Flags--;
		}

		data->angle = CreatureTurn(&item, creature->MaxTurn);
		switch (item.Animation.ActiveState)
		{
		case SOPHIALEIGH_STATE_LAUGH:
			creature->MaxTurn = 0;
			RotateTowardTarget(item, ai, SOPHIALEIGH_WALK_TURN_RATE_MAX);
			break;

		case SOPHIALEIGH_STATE_STAND:
			creature->MaxTurn = 0;
			creature->Flags = 0;

			if (creature->Enemy->IsLara() && creature->Enemy->HitPoints <= 0)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_LAUGH;
			}
			else if (creature->ReachedGoal)
			{
				if (item.ItemFlags[4] == 1)
				{
					item.Animation.TargetState = SOPHIALEIGH_STATE_BIG_SHOOT;
				}
				else if (item.Timer <= 0)
				{
					item.Animation.TargetState = SOPHIALEIGH_STATE_SUMMON;
				}
				else
				{
					item.Animation.TargetState = SOPHIALEIGH_STATE_SMALL_SHOOT;
				}
			}
			else
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_RUN;
			}

			break;

		case SOPHIALEIGH_STATE_WALK:
			creature->MaxTurn = SOPHIALEIGH_WALK_TURN_RATE_MAX;

			if (creature->ReachedGoal)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_STAND;
			}
			else if (ai.distance > SOPHIALEIGH_WALK_RANGE)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_RUN;
			}

			break;

		case SOPHIALEIGH_STATE_RUN:
			creature->MaxTurn = SOPHIALEIGH_RUN_TURN_RATE_MAX;
			data->tilt = data->angle / 2;

			if (creature->ReachedGoal)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_STAND;
				break;
			}
			break;

		case SOPHIALEIGH_STATE_SUMMON:
			creature->MaxTurn = 0;
			RotateTowardTarget(item, ai, SOPHIALEIGH_WALK_TURN_RATE_MAX);

			if (item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SUMMON_START)
			{
				if (item.Animation.FrameNumber == 0)
				{
					item.Timer = SOPHIALEIGH_CHARGE_TIMER_DURATION;
					data->shockwaveTimer = 0;
					data->shockwaveCount = 0;
				}
				else if (item.HitStatus && item.Animation.TargetState != SOPHIALEIGH_STATE_STAND)
				{
					item.Animation.TargetState = SOPHIALEIGH_STATE_STAND;

					StopSoundEffect(SFX_TR3_SOFIALEIGH_SUMMON);
					SoundEffect(SFX_TR3_SOFIALEIGH_SUMMON_FAIL, &item.Pose);
					SoundEffect(SFX_TR3_SOFIALEIGH_TAKE_HIT, &item.Pose);
				}
			}
			else if (item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SUMMON &&
					item.Animation.FrameNumber >= (GetFrameCount(item) - 2))
			{
				// Charged state.
				item.ItemFlags[4] = 1;
			}

			if (!data->shockwaveTimer && data->shockwaveCount < 4)
			{
				sphere = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(2), 0.0f), BLOCK(1 / 16.0f));
				shockwavePos = Pose(Random::GeneratePointInSphere(sphere), item.Pose.Orientation);

				SpawnSophiaSparks(shockwavePos.Position.ToVector3(), Vector3(SOPHIALEIGH_EFFECT_COLOR.x * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.y * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.z * UCHAR_MAX), 5, 2);
				TriggerShockwave(
					&shockwavePos, SOPHIALEIGH_SHOCKWAVE_OUTER_SIZE, SOPHIALEIGH_SHOCKWAVE_INNER_SIZE, SOPHIALEIGH_SHOCKWAVE_SPEED,
					SOPHIALEIGH_EFFECT_COLOR.x * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.y * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.z * UCHAR_MAX,
					36, EulerAngles(Random::GenerateInt(0, 180), 30, Random::GenerateInt(0, 180)), 0, false, true, false, (int)ShockwaveStyle::Sophia);

				data->shockwaveTimer = 2;
				data->shockwaveCount++;
				break;
			}

			if (data->shockwaveCount == 4)
			{
				data->shockwaveCount = 0;
				data->shockwaveTimer = 15;
				break;
			}

			data->shockwaveTimer--;
			break;

		case SOPHIALEIGH_STATE_BIG_SHOOT:
			// Bolt has been shot, reset flag.
			item.ItemFlags[4] = 0;
			creature->MaxTurn = 0;
			RotateTowardTarget(item, ai, SOPHIALEIGH_WALK_TURN_RATE_MAX);

			if (ai.ahead)
			{
				data->torsoYAngle = ai.angle;
				data->torsoXAngle = ai.xAngle;
			}

			if (item.Animation.FrameNumber == 36)
			{
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighRightBite, data, false, SOPHIALEIGH_LASER_DISPERSION_ANGLE);
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighStaffBite, data, true, 0);
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighLeftBite, data, false, -SOPHIALEIGH_LASER_DISPERSION_ANGLE);
			}
			
			break;

		case SOPHIALEIGH_STATE_SMALL_SHOOT:
			creature->MaxTurn = 0;
			RotateTowardTarget(item, ai, SOPHIALEIGH_WALK_TURN_RATE_MAX);

			if (ai.ahead)
			{
				data->torsoYAngle = ai.angle;
				data->torsoXAngle = ai.xAngle;
			}

			if (item.Animation.FrameNumber == 14)
			{
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighRightBite, data, false, SOPHIALEIGH_LASER_DISPERSION_ANGLE);
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighLeftBite, data, false, -SOPHIALEIGH_LASER_DISPERSION_ANGLE);
			}

			break;
		}
	}

	// TR3 Gold Behaviour, which let Sophia attack and chase the player normally.
	static void SophiaLeighNormalControl(ItemInfo& item, CreatureInfo* creature, SophiaData* data)
	{
		AI_INFO ai;
		CreatureAIInfo(&item, &ai);

		auto sphere = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(2), 0.0f), BLOCK(1 / 16.0f));
		auto shockwavePos = Pose(Random::GeneratePointInSphere(sphere), item.Pose.Orientation);
		TriggerSophiaLeightLight(item, shockwavePos.Position.ToVector3());

		// Charge count. Sophia can start charge animation again when at 0.
		if (item.Timer > 0)
			item.Timer--;

		if (ai.ahead)
		{
			data->headAngle = ai.angle;
		}
		else
		{
			data->torsoXAngle = 0;
			data->torsoYAngle = 0;
		}

		GetCreatureMood(&item, &ai, true);
		CreatureMood(&item, &ai, true);

		data->angle = CreatureTurn(&item, creature->MaxTurn);
		switch (item.Animation.ActiveState)
		{
		case SOPHIALEIGH_STATE_LAUGH:
			creature->MaxTurn = 0;
			RotateTowardTarget(item, ai, SOPHIALEIGH_WALK_TURN_RATE_MAX);
			break;

		case SOPHIALEIGH_STATE_STAND:
			creature->MaxTurn = 0;
			creature->Flags = 0;

			if (creature->Enemy->IsLara() && creature->Enemy->HitPoints <= 0)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_LAUGH;
			}
			else if (ai.distance < SOPHIALEIGH_NORMAL_ATTACK_RANGE && Targetable(&item, &ai))
			{
				if (item.ItemFlags[4] == 1)
				{
					item.Animation.TargetState = SOPHIALEIGH_STATE_BIG_SHOOT;
				}
				else if (item.Timer <= 0)
				{
					item.Animation.TargetState = SOPHIALEIGH_STATE_SUMMON;
				}
				else
				{
					item.Animation.TargetState = SOPHIALEIGH_STATE_SMALL_SHOOT;
				}
			}
			else if (ai.distance < SOPHIALEIGH_NORMAL_WALK_RANGE && abs(ai.verticalDistance) <= STEPUP_HEIGHT)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_WALK;
			}
			else
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_RUN;
			}

			break;

		case SOPHIALEIGH_STATE_WALK:
			creature->MaxTurn = SOPHIALEIGH_WALK_TURN_RATE_MAX;

			if (ai.distance > SOPHIALEIGH_NORMAL_WALK_RANGE)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_RUN;
			}
			else if (Targetable(&item, &ai) && ai.distance < SOPHIALEIGH_NORMAL_ATTACK_RANGE)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_STAND;
			}

			break;

		case SOPHIALEIGH_STATE_RUN:
			creature->MaxTurn = SOPHIALEIGH_RUN_TURN_RATE_MAX;
			data->tilt = data->angle / 2;

			if (Targetable(&item, &ai) && ai.distance < SOPHIALEIGH_NORMAL_ATTACK_RANGE)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_STAND;
			}
			else if (ai.distance < SOPHIALEIGH_NORMAL_WALK_RANGE && abs(ai.verticalDistance) <= STEPUP_HEIGHT)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_WALK;
			}

			break;

		case SOPHIALEIGH_STATE_SUMMON:
			creature->MaxTurn = 0;

			if (item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SUMMON_START)
			{
				if (item.Animation.FrameNumber == 0)
				{
					item.Timer = SOPHIALEIGH_CHARGE_TIMER_DURATION;
					data->shockwaveTimer = 0;
					data->shockwaveCount = 0;
				}
				else if (item.HitStatus &&
					item.Animation.TargetState != SOPHIALEIGH_STATE_STAND &&
					Random::TestProbability(1.0f / 50.0f)) // Avoid cancellation every time.
				{
					item.Animation.TargetState = SOPHIALEIGH_STATE_STAND;

					StopSoundEffect(SFX_TR3_SOFIALEIGH_SUMMON);
					SoundEffect(SFX_TR3_SOFIALEIGH_SUMMON_FAIL, &item.Pose);
					SoundEffect(SFX_TR3_SOFIALEIGH_TAKE_HIT, &item.Pose);
				}
			}
			else if (item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SUMMON &&
				item.Animation.FrameNumber >= (GetFrameCount(item) - 2))
			{
				// Charged state.
				item.ItemFlags[4] = 1;
			}

			if (!data->shockwaveTimer && data->shockwaveCount < 4)
			{
				sphere = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(2), 0.0f), BLOCK(1 / 16.0f));
				shockwavePos = Pose(Random::GeneratePointInSphere(sphere), item.Pose.Orientation);

				auto pos = Pose(item.Pose.Position, EulerAngles::Identity);

				SpawnSophiaSparks(shockwavePos.Position.ToVector3(), Vector3(SOPHIALEIGH_EFFECT_COLOR.x * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.y * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.z * UCHAR_MAX), 5, 2);
				TriggerShockwave(&shockwavePos, SOPHIALEIGH_SHOCKWAVE_INNER_SIZE, SOPHIALEIGH_SHOCKWAVE_OUTER_SIZE, SOPHIALEIGH_SHOCKWAVE_SPEED,
					SOPHIALEIGH_EFFECT_COLOR.x * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.y * UCHAR_MAX, SOPHIALEIGH_EFFECT_COLOR.z * UCHAR_MAX,
					36, EulerAngles(Random::GenerateInt(0, 180), 30, Random::GenerateInt(0, 180)), 0, false, true, false, (int)ShockwaveStyle::Sophia);

				data->shockwaveTimer = 2;
				data->shockwaveCount++;
				break;
			}

			if (data->shockwaveCount == 4)
			{
				data->shockwaveCount = 0;
				data->shockwaveTimer = 15;
				break;
			}

			data->shockwaveTimer--;
			break;

		case SOPHIALEIGH_STATE_BIG_SHOOT:
			item.ItemFlags[4] = 0; // Bolt have been shoot, reset the flag.
			creature->MaxTurn = 0;

			if (ai.ahead)
			{
				data->torsoYAngle = ai.angle;
				data->torsoXAngle = ai.xAngle;
			}

			if (item.Animation.FrameNumber == 36)
			{
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighRightBite, data, false, SOPHIALEIGH_LASER_DISPERSION_ANGLE);
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighStaffBite, data, true, 0);
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighLeftBite, data, false, -SOPHIALEIGH_LASER_DISPERSION_ANGLE);
			}

			break;

		case SOPHIALEIGH_STATE_SMALL_SHOOT:
			creature->MaxTurn = 0;
			RotateTowardTarget(item, ai, SOPHIALEIGH_WALK_TURN_RATE_MAX);

			if (ai.ahead)
			{
				data->torsoYAngle = ai.angle;
				data->torsoXAngle = ai.xAngle;
			}

			if (item.Animation.FrameNumber == 14)
			{
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighRightBite, data, false, SOPHIALEIGH_LASER_DISPERSION_ANGLE);
				SpawnSophiaLeighProjectileBolt(item, creature->Enemy, SophiaLeighLeftBite, data, false, -SOPHIALEIGH_LASER_DISPERSION_ANGLE);
			}

			break;
		}
	}

	void InitializeSophiaLeigh(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		CheckForRequiredObjects(item);						// ItemFlags[0] is used.
		item.ItemFlags[1] = 0;								// Light timer (for smoothing).
		item.ItemFlags[4] = 0;								// Charged state (true or false).
		item.ItemFlags[5] = 0;								// Death count.
		item.ItemFlags[7] = 0;								// Explode count.
		SetAnimation(item, SOPHIALEIGH_ANIM_SUMMON_START); // Always starts with projectile attack.
	}

	void SophiaLeighControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& object = Objects[item.ObjectNumber];
		auto& creature = *GetCreatureInfo(&item);

		auto data = SophiaData();

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != SOPHIALEIGH_STATE_DEATH)
				SetAnimation(item, SOPHIALEIGH_ANIM_DEATH);

			int endFrameNumber = GetAnimData(object, SOPHIALEIGH_ANIM_DEATH).EndFrameNumber;
			if (item.Animation.FrameNumber >= endFrameNumber)
			{
				// Avoid having the object stop working.
				item.Animation.FrameNumber = endFrameNumber;
				item.MeshBits.ClearAll();

				if (item.ItemFlags[7] < SOPHIALEIGH_EXPLOSION_NUM_MAX)
					item.ItemFlags[7]++;

				// Do explosion effect.
				ExplodeBoss(itemNumber, item, SOPHIALEIGH_EXPLOSION_NUM_MAX, SOPHIALEIGH_SHOCKWAVE_COLOR, SOPHIALEIGH_EXPLOSION_MAIN_COLOR, SOPHIALEIGH_EXPLOSION_SECOND_COLOR, false);
				return;
			}
		}
		else
		{
			if (item.TriggerFlags == (int)SophiaOCB::Tower ||
				item.TriggerFlags == (int)SophiaOCB::TowerWithVolume)
			{
				SophiaLeighTowerControl(item, &creature, &data);
			}
			else
			{
				SophiaLeighNormalControl(item, &creature, &data);
			}
		}

		CreatureTilt(&item, data.tilt);
		CreatureJoint(&item, 0, data.torsoYAngle);
		CreatureJoint(&item, 1, data.torsoXAngle);
		CreatureJoint(&item, 2, data.headAngle);

		if ((item.Animation.ActiveState < SOPHIALEIGH_STATE_CLIMB2 || item.Animation.ActiveState > SOPHIALEIGH_STATE_FALL4CLICK) &&
			 item.Animation.ActiveState != SOPHIALEIGH_STATE_DEATH)
		{
			switch (CreatureVault(itemNumber, data.angle, 2, SOPHIALEIGH_VAULT_SHIFT))
			{
			case 2:
				creature.MaxTurn = 0;
				SetAnimation(item, SOPHIALEIGH_ANIM_CLIMB2CLICK);
				break;

			case 3:
				creature.MaxTurn = 0;
				SetAnimation(item, SOPHIALEIGH_ANIM_CLIMB3CLICK);
				break;

			case 4:
				creature.MaxTurn = 0;
				SetAnimation(item, SOPHIALEIGH_ANIM_CLIMB4CLICK);
				break;

			case -4:
				creature.MaxTurn = 0;
				SetAnimation(item, SOPHIALEIGH_ANIM_FALL4CLICK);
				break;
			}
		}
		else
		{
			CreatureAnimation(itemNumber, data.angle, 0);
		}
	}

	void SpawnSophiaSparks(const Vector3& pos, const Vector3& color, unsigned int count, int multiplier)
	{
		for (int i = 0; i < count; i++)
		{
			auto* spark = GetFreeParticle();
			auto sphere = BoundingSphere(Vector3::Zero, BLOCK(2));
			auto mulSqr = SQUARE(multiplier);
			auto vel = Random::GeneratePointInSphere(sphere) * mulSqr;

			spark->on = true;
			spark->sR = color.x;
			spark->sG = color.y;
			spark->sB = color.z;
			spark->dB = 0;
			spark->dG = 0;
			spark->dR = 0;
			spark->colFadeSpeed = mulSqr * 9;
			spark->fadeToBlack = 0;
			spark->life =
			spark->sLife = mulSqr * 9;
			spark->blendMode = BlendMode::Additive;
			spark->x = pos.x;
			spark->y = pos.y;
			spark->z = pos.z;
			spark->gravity = Random::GenerateInt(0, 32);
			spark->yVel = vel.x;
			spark->xVel = vel.y;
			spark->zVel = vel.z;
			spark->flags = SP_NONE;
			spark->maxYvel = 0;
			spark->friction =  mulSqr * 34;
			spark->scalar = 3;
			spark->dSize =
			spark->sSize =
			spark->size = Random::GenerateInt(84, 98);
		}
	}
}
