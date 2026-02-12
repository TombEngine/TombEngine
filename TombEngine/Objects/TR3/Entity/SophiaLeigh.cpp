#include "framework.h"
#include "Objects/TR3/Entity/SophiaLeigh.h"

#include "Game/Animation/Animation.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Game/pickup/pickup.h"
#include "Objects/Effects/Boss.h"
#include "Objects/Effects/enemy_missile.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Boss;
using namespace TEN::Entities::Effects;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto SOPHIALEIGH_WALK_RANGE = SQUARE(BLOCK(1));
	constexpr auto SOPHIALEIGH_NORMAL_ATTACK_RANGE = SQUARE(BLOCK(5));
	constexpr auto SOPHIALEIGH_NORMAL_WALK_RANGE = SQUARE(BLOCK(5));
	constexpr auto SOPHIALEIGH_Y_DISTANCE_RANGE = BLOCK(1.5f);
	constexpr auto SOPHIALEIGH_REACHED_GOAL_RANGE = BLOCK(0.5f);
	constexpr auto SOPHIALEIGH_KNOCKBACK_RANGE = BLOCK(3);
	constexpr auto SOPHIALEIGH_WAYPOINT_STABLE_FRAMES = 15; // Hysteresis: frames before committing to waypoint change.

	constexpr auto SOPHIALEIGH_DAMAGE_SMALL_BOLT = 4;
	constexpr auto SOPHIALEIGH_DAMAGE_LARGE_BOLT = 10;

	constexpr auto SOPHIALEIGH_CHARGE_TIMER_DURATION = 600;
	constexpr auto SOPHIALEIGH_EXPLOSION_NUM_MAX = 60;

	constexpr auto SOPHIALEIGH_EFFECT_COLOR = Vector4(0.0f, 0.7f, 0.3f, 0.5f);
	constexpr auto SOPHIALEIGH_SHOCKWAVE_COLOR = Vector4(0.0f, 0.7f, 0.3f, 0.5f);
	constexpr auto SOPHIALEIGH_EXPLOSION_MAIN_COLOR = Vector4(0.0f, 0.7f, 0.2f, 0.5f);
	constexpr auto SOPHIALEIGH_EXPLOSION_SECOND_COLOR = Vector4(0.0f, 0.7f, 0.0f, 0.5f);

	constexpr auto SOPHIALEIGH_WALK_TURN_RATE_MAX = ANGLE(4.0f);
	constexpr auto SOPHIALEIGH_RUN_TURN_RATE_MAX = ANGLE(7.0f);
	constexpr auto SOPHIALEIGH_LASER_DECREASE_XANGLE_IF_LARA_CROUCH = ANGLE(0.2f);
	constexpr auto SOPHIALEIGH_LASER_DISPERSION_ANGLE = ANGLE(1.5f);

	constexpr auto SOPHIALEIGH_LIGHTNING_GLOW_SIZE = 8;
	constexpr auto SOPHIALEIGH_MAX_LIGHTNING_GLOW_SIZE = 10;
	constexpr auto SOPHIALEIGH_SHOCKWAVE_SPEED = -184;
	constexpr auto SOPHIALEIGH_SHOCKWAVE_INNER_SIZE = 2700;
	constexpr auto SOPHIALEIGH_SHOCKWAVE_OUTER_SIZE = 2300;

	constexpr auto SOPHIALEIGH_KNOCKBACK_LARGE_INNER_SIZE = 800;
	constexpr auto SOPHIALEIGH_KNOCKBACK_LARGE_OUTER_SIZE = 0;
	constexpr auto SOPHIALEIGH_KNOCKBACK_SMALL_INNER_SIZE = 200;
	constexpr auto SOPHIALEIGH_KNOCKBACK_SMALL_OUTER_SIZE = -400;

	constexpr auto SOPHIALEIGH_VAULT_SHIFT = 96;

	const auto SophiaLeighStaffBite = CreatureBiteInfo(Vector3(-28, 56, 356), 10);
	const auto SophiaLeighLeftBite = CreatureBiteInfo(Vector3(-72, 48, 356), 10);
	const auto SophiaLeighRightBite = CreatureBiteInfo(Vector3(16, 48, 304), 10);

	struct SophiaData
	{
		short angle = 0;
		short tilt = 0;
		short headAngle = 0;
		short torsoXAngle = 0;
		short torsoYAngle = 0;
		short chargeDelay = 0;
		short shockwaveTimer = 0;
		short shockwaveCount = 0;
		short waypointStableTimer = 0;  // Hysteresis timer to prevent waypoint oscillation.
		short pendingLocationAI = -1;   // Pending LocationAI value waiting for stability.
	};

	static std::unordered_map<int, SophiaData> SophiaLeighs = {};

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
		Normal = 0,				// Move, climb, attack, and chase player.
		Tower = 1,				// TR3 one, with climbing only.
		TowerLua = 2,			// TR3 one, but uses lua to move. Must increase/decrease creature->LocationAI to go up/down.
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

		if (!item.IsLara()) return; // Set Lara to fall back.
		SetAnimation(item, LA_FALL_BACK);
	}

	static void TriggerKnockback(ItemInfo& item, int life = 32)
	{
		auto& creature = *GetCreatureInfo(&item);
		auto& enemy = *creature.Enemy;
		if (enemy.ObjectNumber == ID_AI_X1) return; // Ignore knockback for AI object used for path, just in case...

		// Fly cheat active, ignore knockback.
		if (enemy.IsLara() && Lara.Control.WaterStatus == WaterStatus::FlyCheat)
			return;

		auto orient = Geometry::GetOrientToPoint(enemy.Pose.Position.ToVector3(), item.Pose.Position.ToVector3());
		auto distance = Vector3::Distance(item.Pose.Position.ToVector3(), enemy.Pose.Position.ToVector3());
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

	// Shared per-frame state-machine logic for tower mode.
	// Called after creature->Enemy and ai have been correctly set for the current situation
	// (ai relative to Lara when at goal, ai relative to AI_X1 waypoint when still navigating).
	static void SophiaLeighTowerControlFrame(ItemInfo& item, CreatureInfo* creature, SophiaData* data, AI_INFO& ai)
	{
		// Charge count. Sophia can start the charge animation again when at 0 and sophia is in stand state.
		if (data->chargeDelay > 0)
			data->chargeDelay--;

		bool isValidTarget = creature->Enemy->IsLara() && creature->Enemy->ObjectNumber != ID_AI_X1; // Avoid AI object as target.
		if (isValidTarget && ai.ahead)
			data->headAngle = ai.angle;

		GetCreatureMood(&item, &ai, true);
		CreatureMood(&item, &ai, true);

		// Knockback the target if Sophia in tower mode.
		// Avoid spawning rings if target is dead.
		if (ai.distance < SQUARE(SOPHIALEIGH_KNOCKBACK_RANGE) && creature->Flags <= 0 && creature->Enemy->HitPoints > 0)
		{
			TriggerKnockback(item);
			creature->Flags = 50;
		}
		else
		{
			if (creature->Flags > 0)
				creature->Flags--;
		}

		auto sphere = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(2), 0.0f), BLOCK(1 / 16.0f));
		auto shockwavePos = Pose(Random::GeneratePointInSphere(sphere), item.Pose.Orientation);

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

			if (isValidTarget && creature->Enemy->HitPoints <= 0)
			{
				item.Animation.TargetState = SOPHIALEIGH_STATE_LAUGH;
			}
			else if (creature->ReachedGoal && ai.ahead && isValidTarget) // Wait for Lara to be in front before firing.
			{
				if (item.ItemFlags[4] == 1) // Charged state, SHOOT IT!
				{
					item.Animation.TargetState = SOPHIALEIGH_STATE_BIG_SHOOT;
				}
				else if (data->chargeDelay <= 0)
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
				item.Animation.TargetState = SOPHIALEIGH_STATE_STAND;

			break;

		case SOPHIALEIGH_STATE_SUMMON:
			creature->MaxTurn = 0;
			RotateTowardTarget(item, ai, SOPHIALEIGH_WALK_TURN_RATE_MAX);

			if (item.Animation.AnimNumber == SOPHIALEIGH_ANIM_SUMMON_START)
			{
				if (item.Animation.FrameNumber == 0)
				{
					data->chargeDelay = SOPHIALEIGH_CHARGE_TIMER_DURATION;
					data->shockwaveCount = 0;
					data->shockwaveTimer = 0;
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
			// Bolt has been shot, reset flag.
			item.ItemFlags[4] = 0;
			creature->MaxTurn = 0;
			RotateTowardTarget(item, ai, SOPHIALEIGH_WALK_TURN_RATE_MAX);

			if (isValidTarget && ai.ahead)
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

	// TR3 Behaviour, which let sophia go to AI_X1 object to move up/down a "tower"
	static void SophiaLeighTowerControl(ItemInfo& item, CreatureInfo* creature, SophiaData* data)
	{
		if (item.AIBits)
			GetAITarget(creature);

		auto sphere = BoundingSphere(item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(2), 0.0f), BLOCK(1 / 16.0f));
		auto shockwavePos = Pose(Random::GeneratePointInSphere(sphere), item.Pose.Orientation);
		TriggerSophiaLeightLight(item, shockwavePos.Position.ToVector3());

		// Set current AI_X1 waypoint as enemy and check if reached.
		FindAITargetObject(creature, ID_AI_X1, creature->LocationAI, false);
		creature->ReachedGoal = Vector3i::Distance(item.Pose.Position, creature->Enemy->Pose.Position) < SOPHIALEIGH_REACHED_GOAL_RANGE;

		if (creature->ReachedGoal)
		{
			item.ItemFlags[6] = 1; // Reached goal.

			// Restore Lara as enemy to compute vertical distance for up/down tower decision.
			creature->Enemy = LaraItem; // TODO: Deal with LaraItem global.

			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			item.ItemFlags[3] = (short)ai.verticalDistance; // Store vertical distance to Lara.

			if (item.TriggerFlags == (int)SophiaOCB::Tower)
			{
				// Update waypoint based on Lara's vertical position with hysteresis to prevent oscillation.

				int desiredLocationAI = creature->LocationAI; // Default: stay at current waypoint.
				bool isInDeadZone = (ai.verticalDistance >= -SOPHIALEIGH_Y_DISTANCE_RANGE &&
					ai.verticalDistance <= SOPHIALEIGH_Y_DISTANCE_RANGE);

				// Determine which direction to go based on Lara's position.
				if (ai.verticalDistance > SOPHIALEIGH_Y_DISTANCE_RANGE)
				{
					desiredLocationAI = creature->LocationAI + 1; // Lara is above.
				}
				else if (ai.verticalDistance < -SOPHIALEIGH_Y_DISTANCE_RANGE)
				{
					// Lara is below - but only go down if not already at bottom.
					if (creature->LocationAI > 0)
						desiredLocationAI = creature->LocationAI - 1;
					else
						isInDeadZone = true; // Treat "can't go lower" as dead zone.
				}

				// Clear pending decision if in dead zone (Lara at similar height or can't move further).
				if (isInDeadZone)
				{
					data->pendingLocationAI = -1;
					data->waypointStableTimer = 0;
				}
				// Apply hysteresis: only change waypoint after consistent direction for multiple frames.
				else if (desiredLocationAI == data->pendingLocationAI)
				{
					data->waypointStableTimer--;

					if (data->waypointStableTimer <= 0)
					{
						creature->LocationAI = desiredLocationAI;
						data->pendingLocationAI = -1;
						data->waypointStableTimer = 0;
					}
				}
				else
				{
					// Direction changed - reset timer and track new pending direction.
					data->pendingLocationAI = desiredLocationAI;
					data->waypointStableTimer = SOPHIALEIGH_WAYPOINT_STABLE_FRAMES;
				}
			}

			// Run state machine with ai relative to Lara.
			SophiaLeighTowerControlFrame(item, creature, data, ai);
		}
		else
		{
			item.ItemFlags[3] = 0; // Clear vertical distance to goal.
			item.ItemFlags[6] = 0; // Not reached goal.

			// Reset hysteresis state while navigating.
			data->pendingLocationAI = -1;
			data->waypointStableTimer = 0;

			// Run state machine with ai relative to waypoint for correct movement/turning.
			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			SophiaLeighTowerControlFrame(item, creature, data, ai);
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
		if (data->chargeDelay > 0)
			data->chargeDelay--;

		bool isValidTarget = (creature->Enemy->IsLara() || creature->Enemy->IsCreature());
		if (isValidTarget && ai.ahead)
			data->headAngle = ai.angle;

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
				else if (data->chargeDelay <= 0)
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
					data->chargeDelay = SOPHIALEIGH_CHARGE_TIMER_DURATION;
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

			if (creature->Enemy->IsLara() && ai.ahead)
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

			if (creature->Enemy->IsLara() && ai.ahead)
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
		CheckForRequiredObjects(item);								// Checks for required boss objects.
		item.ItemFlags[1] = 0;										// Light timer (for smoothing).
		item.ItemFlags[3] = 0;										// Target vertical distance.
		item.ItemFlags[4] = 0;										// Charged state (true or false).
		item.ItemFlags[5] = 0;										// Death count.
		item.ItemFlags[6] = 0;										// Reached goal (true or false).
		item.SetFlagField((int)BossItemFlags::ExplodeCount, 0);	// Explosion counter.
		SetAnimation(item, SOPHIALEIGH_ANIM_SUMMON_START);			// Always starts with projectile attack.

		// Avoid multiple initialization of same item.
		if (SophiaLeighs.find(itemNumber) == SophiaLeighs.end())
			SophiaLeighs.insert({ itemNumber, SophiaData() });
	}

	void SophiaLeighControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;
		if (SophiaLeighs.find(itemNumber) == SophiaLeighs.end())
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& data = SophiaLeighs.at(itemNumber);
		auto& object = Objects[item.ObjectNumber];
		auto& creature = *GetCreatureInfo(&item);

		// These values are reset each frame.
		data.angle = 0;
		data.tilt = 0;
		data.headAngle = 0;
		data.torsoYAngle = 0;
		data.torsoXAngle = 0;

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

				if (item.GetFlagField((int)BossItemFlags::ExplodeCount) < SOPHIALEIGH_EXPLOSION_NUM_MAX)
					item.ItemFlags[(int)BossItemFlags::ExplodeCount]++;

				// Explosion effect with pickup drop on completion.
				ExplodeBoss(itemNumber, item, SOPHIALEIGH_EXPLOSION_NUM_MAX, SOPHIALEIGH_SHOCKWAVE_COLOR, SOPHIALEIGH_EXPLOSION_MAIN_COLOR, SOPHIALEIGH_EXPLOSION_SECOND_COLOR, true);
				return;
			}
		}
		else
		{
			if (item.TriggerFlags == (int)SophiaOCB::Tower ||
				item.TriggerFlags == (int)SophiaOCB::TowerLua)
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

	void SophiaLeighHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		// In tower mode, except from trigger, Sophia is immune to damage and any effects (like fire) (if not dead).
		if ((target.TriggerFlags == (int)SophiaOCB::Tower || target.TriggerFlags == (int)SophiaOCB::TowerLua) && target.HitPoints > 0)
		{
			target.Effect.Count = 0;
			target.Effect.Type = EffectType::None;
			return;
		}

		DefaultItemHit(target, source, pos, damage, isExplosive, jointIndex);
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
			spark->friction = mulSqr * 34;
			spark->scalar = 3;
			spark->dSize =
				spark->sSize =
				spark->size = Random::GenerateInt(84, 98);
		}
	}

	void ClearSophiaLeighs()
	{
		SophiaLeighs.clear();
	}
}
