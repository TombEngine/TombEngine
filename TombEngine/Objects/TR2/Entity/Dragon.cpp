#include "framework.h"
#include "Objects/TR2/Entity/Dragon.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/pickup/pickup.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/clock.h"
#include "Specific/Input/Input.h"

using namespace TEN::Collision::Point;
using namespace TEN::Input;
using namespace TEN::Math;

// NOTES:
// OCB 0: Dragon dies when hitpoints reach 0.
// OCB 1: Dragon dies when player retrieves dagger.
// 
// item.ItemFlags[0]: Back segment item number.
// item.ItemFlags[1]: Timer for temporary defeat and death in frame time.

namespace TEN::Entities::Creatures::TR2
{
	auto DragonDaggerBounds = ObjectCollisionBounds
	{
		GameBoundingBox::Zero,
		std::pair(
			EulerAngles(ANGLE(-10.0f), -LARA_GRAB_THRESHOLD + ANGLE(90.0f) , ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), LARA_GRAB_THRESHOLD + ANGLE(90.0f), ANGLE(10.0f)))
	};

	auto DragonDaggerPos = Vector3i::Zero;

	constexpr auto DRAGON_SWIPE_ATTACK_DAMAGE = 250;
	constexpr auto DRAGON_CONTACT_DAMAGE	  = 10;

	constexpr auto DRAGON_NEAR_RANGE = SQUARE(BLOCK(3));
	constexpr auto DRAGON_IDLE_RANGE = SQUARE(BLOCK(6));

	constexpr auto DRAGON_WALK_TURN_RATE_MAX   = ANGLE(2.0f);
	constexpr auto DRAGON_TURN_THRESHOLD_ANGLE = ANGLE(1.0f);

	constexpr auto DRAGON_LIVE_TIME	  = 11 * FPS;
	constexpr auto DRAGON_ALMOST_LIVE = 100;

	const auto DragonMouthBite = CreatureBiteInfo(Vector3(35.0f, 171.0f, 1168.0f), 12);
	const auto DragonBackSpineJoints		= std::vector<unsigned int>{ 21, 22, 23 };
	const auto DragonSwipeAttackJointsLeft	= std::vector<unsigned int>{ 24, 25, 26, 27, 28, 29, 30 };
	const auto DragonSwipeAttackJointsRight = std::vector<unsigned int>{ 1, 2, 3, 4, 5, 6, 7 };

	enum class DragonLightEffectType
	{
		Yellow,
		Red
	};

	enum DragonState
	{
		// No state 0.
		DRAGON_STATE_WALK = 1,
		DRAGON_STATE_MOVE_LEFT = 2,
		DRAGON_STATE_MOVE_RIGHT = 3,
		DRAGON_STATE_AIM_1 = 4,
		DRAGON_STATE_FIRE_1 = 5,
		DRAGON_STATE_IDLE = 6,
		DRAGON_STATE_TURN_LEFT = 7,
		DRAGON_STATE_TURN_RIGHT = 8,
		DRAGON_STATE_SWIPE_ATTACK_LEFT = 9,
		DRAGON_STATE_SWIPE_ATTACK_RIGHT = 10,
		DRAGON_STATE_DEFEAT = 11
	};

	enum DragonAnim
	{
		DRAGON_ANIM_WALK = 0,
		DRAGON_ANIM_WALK_TO_MOVE_LEFT = 1,
		DRAGON_ANIM_MOVE_LEFT = 2,
		DRAGON_ANIM_MOVE_LEFT_TO_WALK = 3,
		DRAGON_ANIM_WALK_TO_MOVE_RIGHT = 4,
		DRAGON_ANIM_MOVE_RIGHT = 5,
		DRAGON_ANIM_MOVE_RIGHT_TO_WALK = 6,
		DRAGON_ANIM_WALK_TO_IDLE = 7,
		DRAGON_ANIM_IDLE = 8,
		DRAGON_ANIM_IDLE_TO_WALK = 9,
		DRAGON_ANIM_IDLE_TO_FIRE = 10,
		DRAGON_ANIM_FIRE = 11,
		DRAGON_ANIM_FIRE_TO_IDLE = 12,
		DRAGON_ANIM_TURNING_LEFT = 13,
		DRAGON_ANIM_TURNING_RIGHT = 14,
		DRAGON_ANIM_ATTACK_LEFT_1 = 15,
		DRAGON_ANIM_ATTACK_LEFT_2 = 16,
		DRAGON_ANIM_ATTACK_LEFT_3 = 17,
		DRAGON_ANIM_ATTACK_RIGHT_1 = 18,
		DRAGON_ANIM_ATTACK_RIGHT_2 = 19,
		DRAGON_ANIM_ATTACK_RIGHT_3 = 20,
		DRAGON_ANIM_DEATH = 21,
		DRAGON_ANIM_DEFEATED = 22,
		DRAGON_ANIM_RECOVER = 23
	};

	enum DragonOCB
	{
		DRAGON_OCB_NORMAL = 0,
		DRAGON_OCB_DAGGER = 1
	};

	static void InitializeDragonBones(const ItemInfo& item)
	{
		int frontBoneItemNumber = SpawnItem(item, ID_DRAGON_BONE_FRONT);
		int backBoneItemNumber = SpawnItem(item, ID_DRAGON_BONE_BACK);

		if (backBoneItemNumber == NO_VALUE || frontBoneItemNumber == NO_VALUE)
		{
			TENLog("Failed to create dragon skeleton objects.", LogLevel::Warning);
			return;
		}
	}

	static void InitializeDragonBack(ItemInfo& frontItem)
	{
		int backItemNumber = SpawnItem(frontItem, ID_DRAGON_BACK);

		if (backItemNumber == NO_VALUE)
		{
			TENLog("Failed to create dragon back body segment.", LogLevel::Warning);
			return;
		}

		auto& backItem = g_Level.Items[backItemNumber];
		backItem.Status = ITEM_INVISIBLE;

		// Mark the bone meshes 21, 23 and 23 for they don't be rendered.
		backItem.MeshBits.Clear(DragonBackSpineJoints);

		// Store back body segment item number.
		frontItem.ItemFlags[0] = backItemNumber;
		backItem.ItemFlags[0] = NO_VALUE;
	}

	void InitializeDragon(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Initialize front body segment.
		InitializeCreature(item.Index);
		SetAnimation(item, DRAGON_ANIM_IDLE);

		// Initialize back body segment.
		InitializeDragonBack(item);
	}

	static void SyncDragonBackSegment(ItemInfo& frontItem)
	{
		short& backItemNumber = frontItem.ItemFlags[0];
		auto& backItem = g_Level.Items[backItemNumber];

		// Sync destruction.
		backItem.Status = frontItem.Status;
		if (backItem.Status == ITEM_DEACTIVATED)
		{
			KillItem(backItem.Index);
			backItemNumber = NO_VALUE;
			return;
		}

		// Sync animation.
		SetAnimation(backItem, frontItem.Animation.AnimNumber, frontItem.Animation.FrameNumber);

		// Sync position.
		backItem.Pose = frontItem.Pose;
		if (backItem.RoomNumber != frontItem.RoomNumber)
			ItemNewRoom(backItem.Index, frontItem.RoomNumber);
	}

	static void SpawnDragonLightEffect(const ItemInfo& item, DragonLightEffectType type)
	{
		auto pos = item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(1), 0.0f);

		switch (type)
		{
		default:
		case DragonLightEffectType::Yellow:
		{
			auto color = Color(
				Random::GenerateFloat(0.8f, 0.9f),
				Random::GenerateFloat(0.4f, 0.5f),
				Random::GenerateFloat(0.2f, 0.3f));
			float falloff = Random::GenerateFloat(BLOCK(6), BLOCK(20));

			SpawnDynamicPointLight(pos, color, falloff);
		}
		break;

		case DragonLightEffectType::Red:
		{
			auto color = Color(
				Random::GenerateFloat(0.8f, 0.9f),
				Random::GenerateFloat(0.2f, 0.3f),
				Random::GenerateFloat(0.0f, 0.1f));
			float falloff = Random::GenerateFloat(BLOCK(6), BLOCK(12));

			SpawnDynamicPointLight(pos, color, falloff);
		}
		break;
		}
	}

	// TODO: Smoke and sparks.
	// TODO: Animate flame sprite sequence.
	static void SpawnDragonFireBreathEffect(const ItemInfo& item, const CreatureBiteInfo& bite)
	{
		constexpr auto FIRE_COUNT	 = 3;
		constexpr auto SPHERE_RADIUS = BLOCK(0.2f);
		constexpr auto VEL			 = 300.0f;

		for (int i = 0; i < FIRE_COUNT; i++)
		{
			auto& fire = *GetFreeParticle();

			auto origin = GetJointPosition(item, bite.BoneID, bite.Position).ToVector3();
			auto target = GetJointPosition(LaraItem, LM_HIPS).ToVector3();

			auto sphere = BoundingSphere(origin, SPHERE_RADIUS);
			auto pos = Random::GeneratePointInSphere(sphere);

			auto dir = target - origin;
			dir.Normalize();
			dir *= VEL;

			fire.SpriteSeqID = ID_FIRE_SPRITES;
			fire.SpriteID = Random::GenerateInt(0, 35);

			fire.x = pos.x;
			fire.y = pos.y;
			fire.z = pos.z;

			fire.on = true;
			fire.sR = Random::GenerateFloat(0.85f, 1.0f) * UCHAR_MAX;
			fire.sG = 0.25f * UCHAR_MAX;
			fire.sB = 0.15f * UCHAR_MAX;
			fire.dR = Random::GenerateFloat(0.5f, 0.75f) * UCHAR_MAX;
			fire.dG = Random::GenerateFloat(0.3f, 0.6f) * UCHAR_MAX;
			fire.dB = 0.15f * UCHAR_MAX;
			fire.colFadeSpeed = 12;
			fire.fadeToBlack = 8;
			fire.blendMode = BlendMode::Additive;

			int v = Random::GenerateFloat(0.75f, 1.0f) * UCHAR_MAX;
			fire.life =
			fire.sLife = v / 6;

			fire.xVel = v * (dir.x) / 10;
			fire.yVel = v * (dir.y) / 10;
			fire.zVel = v * (dir.z) / 10;

			fire.friction = 85;
			fire.gravity = -Random::GenerateInt(-16, 16);
			fire.maxYvel = 0;
			fire.flags = SP_FIRE | SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;

			fire.scalar = 6;
			fire.dSize = (v * Random::GenerateFloat(60.0f, 67.0f)) / BLOCK(0.25f);
			fire.sSize = fire.dSize / 4;
			fire.size = fire.dSize;
		}
	}

	static void SpawnDragonSmokeBreathEffect(const ItemInfo& item, const CreatureBiteInfo& bite)
	{
		constexpr auto SMOKE_COUNT	 = 2;
		constexpr auto SPHERE_RADIUS = BLOCK(0.25f);

		for (int i = 0; i < SMOKE_COUNT; i++)
		{
			auto origin = GetJointPosition(item, bite.BoneID, bite.Position).ToVector3();
			auto target = GetJointPosition(LaraItem, LM_HIPS).ToVector3();

			auto sphere = BoundingSphere(origin, SPHERE_RADIUS);
			auto pos = Random::GeneratePointInSphere(sphere);
			auto vel = Random::GenerateDirection() * Random::GenerateFloat(0.0f, 1.0f);

			auto dir = target - origin;
			dir.Normalize();

			int spriteID = Random::GenerateInt(0, 10);

			auto& smoke = *GetFreeParticle();

			smoke.SpriteSeqID = ID_SMOKE_SPRITES;
			smoke.SpriteID = Random::GenerateInt(0, 10);
			smoke.on = true;
			smoke.x = pos.x;
			smoke.y = pos.y;
			smoke.z = pos.z;
			smoke.yVel = vel.x;
			smoke.xVel = vel.y;
			smoke.zVel = vel.z;
			smoke.sR = 0.5f * UCHAR_MAX;
			smoke.sG = 0.5f * UCHAR_MAX;
			smoke.sB = 0.5f * UCHAR_MAX;
			smoke.dR = 0.5f * UCHAR_MAX;
			smoke.dG = 0.25f * UCHAR_MAX;
			smoke.dB = 0;
			smoke.colFadeSpeed = 128;
			smoke.fadeToBlack = 64;
			smoke.blendMode = BlendMode::Additive;
			smoke.life = smoke.sLife = Random::GenerateInt(10, 60);

			smoke.friction = 0;
			smoke.gravity = -30;
			smoke.maxYvel = 5;
			smoke.flags = SP_DEF | SP_ROTATE | SP_FIRE;

			smoke.scalar = 0.1f;  
			smoke.dSize = BLOCK(0.1f);  
			smoke.sSize = BLOCK(0.5f);  
			smoke.size = smoke.dSize/8;
			smoke.flags = SP_WIND | SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
		}
	}

	static void SpawnDragonShockwaveEffect(const ItemInfo& item, int jointIndex)
	{
		auto pos = GetJointPosition(item, jointIndex, Vector3i(0, -8, 0));

		if (item.Animation.AnimNumber == DRAGON_ANIM_ATTACK_LEFT_2 ||
			item.Animation.AnimNumber == DRAGON_ANIM_ATTACK_RIGHT_2)
		{
			if (item.Animation.FrameNumber == GetAnimData(item).EndFrameNumber)
			{
				auto pointColl = GetPointCollision(pos, item.RoomNumber);

				if (pointColl.GetFloorHeight() == NO_HEIGHT)
				{
					pos.y -= CLICK(0.5f);
				}
				else
				{
					pos.y = pointColl.GetFloorHeight() - CLICK(0.5f);
				}

				auto pose = Pose(pos, EulerAngles::Identity);
				TriggerShockwave(&pose, 24, 88, 256, 128, 128, 128, 32, EulerAngles::Identity, 8, true, false, false, (int)ShockwaveStyle::Normal);

				Camera.bounce = -128;
			}
		}
	}

	void ControlDragon(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);
		auto& timer = item.ItemFlags[1];

		short headingAngle = 0;
		short headOrient = 0;

		bool isTargetAhead = false;

		// NOTE: OCB 1 = player must retrieve dagger to kill dragon.
		bool flagDaggerDeath = (item.TriggerFlags == DRAGON_OCB_DAGGER) ? true : false;

		if (item.HitPoints <= 0)
		{
			if (item.Animation.ActiveState != DRAGON_STATE_DEFEAT)
			{
				SetAnimation(item, DRAGON_ANIM_DEATH);
				timer = 0;
			}
			else
			{
				// Temporary defeat.
				if (timer >= 0)
				{
					SpawnDragonLightEffect(item, DragonLightEffectType::Yellow);
					timer++;

					if (timer == DRAGON_LIVE_TIME)
						item.Animation.TargetState = DRAGON_STATE_IDLE;

					if (timer == DRAGON_LIVE_TIME + DRAGON_ALMOST_LIVE)
						item.HitPoints = Objects[ID_DRAGON_FRONT].HitPoints / 2;

					if (!flagDaggerDeath)
					{
						if (item.Animation.AnimNumber == DRAGON_ANIM_DEFEATED)
							timer = NO_VALUE;
					}
				}
				// Death.
				else
				{
					if (timer > -20)
						SpawnDragonLightEffect(item, DragonLightEffectType::Red);

					if (timer == -100)
					{
						InitializeDragonBones(item);

						if (flagDaggerDeath)
						{
							CollectCarriedItems(&item);
						}
					}
					else if (timer == -200)
					{
						DisableEntityAI(itemNumber);
						KillItem(itemNumber);

						if (!flagDaggerDeath)
						{
							DropPickups(&item);
						}

						item.Status = ITEM_DEACTIVATED;
					}
					else if (timer < -100)
					{
						item.Pose.Position.y += 10;
					}

					timer--;
				}
			}
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(&item, &ai);

			GetCreatureMood(&item, &ai, true);
			CreatureMood(&item, &ai, true);
			headingAngle = CreatureTurn(&item, DRAGON_WALK_TURN_RATE_MAX);

			isTargetAhead = (ai.ahead && ai.distance > DRAGON_NEAR_RANGE && ai.distance < DRAGON_IDLE_RANGE);

			if (item.TouchBits.TestAny())
				DoDamage(creature.Enemy, DRAGON_CONTACT_DAMAGE);

			switch (item.Animation.ActiveState)
			{
			case DRAGON_STATE_IDLE:
				item.Pose.Orientation.y -= headingAngle;

				if (!isTargetAhead)
				{
					if (ai.distance > DRAGON_IDLE_RANGE || !ai.ahead)
					{
						item.Animation.TargetState = DRAGON_STATE_WALK;
					}
					else if (ai.ahead && ai.distance < DRAGON_NEAR_RANGE && !creature.Flags)
					{
						creature.Flags = 1;

						if (ai.angle < 0)
						{
							item.Animation.TargetState = DRAGON_STATE_SWIPE_ATTACK_LEFT;
						}
						else
						{
							item.Animation.TargetState = DRAGON_STATE_SWIPE_ATTACK_RIGHT;
						}
					}
					else if (ai.angle < 0)
					{
						item.Animation.TargetState = DRAGON_STATE_TURN_LEFT;
					}
					else
					{
						item.Animation.TargetState = DRAGON_STATE_TURN_RIGHT;
					}
				}
				else
				{
					item.Animation.TargetState = DRAGON_STATE_AIM_1;
				}

				break;

			case DRAGON_STATE_SWIPE_ATTACK_LEFT:
				if (item.TouchBits.Test(DragonSwipeAttackJointsLeft))
				{
					DoDamage(creature.Enemy, DRAGON_SWIPE_ATTACK_DAMAGE);
					creature.Flags = 0;
				}

				SpawnDragonShockwaveEffect(item, DragonSwipeAttackJointsLeft[3]);

				break;

			case DRAGON_STATE_SWIPE_ATTACK_RIGHT:
				if (item.TouchBits.Test(DragonSwipeAttackJointsRight))
				{
					DoDamage(creature.Enemy, DRAGON_SWIPE_ATTACK_DAMAGE);
					creature.Flags = 0;
				}

				SpawnDragonShockwaveEffect(item, DragonSwipeAttackJointsRight[3]);

				break;

			case DRAGON_STATE_WALK:
				creature.Flags = 0;

				if (isTargetAhead)
				{
					item.Animation.TargetState = DRAGON_STATE_IDLE;
				}
				else if (headingAngle < -DRAGON_TURN_THRESHOLD_ANGLE)
				{
					if (ai.distance < DRAGON_IDLE_RANGE && ai.ahead)
					{
						item.Animation.TargetState = DRAGON_STATE_IDLE;
					}
					else
					{
						item.Animation.TargetState = DRAGON_STATE_MOVE_LEFT;
					}
				}
				else if (headingAngle > DRAGON_TURN_THRESHOLD_ANGLE)
				{
					if (ai.distance < DRAGON_IDLE_RANGE && ai.ahead)
					{
						item.Animation.TargetState = DRAGON_STATE_IDLE;
					}
					else
					{
						item.Animation.TargetState = DRAGON_STATE_MOVE_RIGHT;
					}
				}

				break;

			case DRAGON_STATE_MOVE_LEFT:
				if (headingAngle > -DRAGON_TURN_THRESHOLD_ANGLE || isTargetAhead)
					item.Animation.TargetState = DRAGON_STATE_WALK;

				break;

			case DRAGON_STATE_MOVE_RIGHT:
				if (headingAngle < DRAGON_TURN_THRESHOLD_ANGLE || isTargetAhead)
					item.Animation.TargetState = DRAGON_STATE_WALK;

				break;

			case DRAGON_STATE_TURN_LEFT:
				item.Pose.Orientation.y -= ANGLE(1.0f) - headingAngle;
				creature.Flags = 0;
				break;

			case DRAGON_STATE_TURN_RIGHT:
				item.Pose.Orientation.y += ANGLE(1.0f) - headingAngle;
				creature.Flags = 0;
				break;

			case DRAGON_STATE_AIM_1:
				item.Pose.Orientation.y -= headingAngle;

				if (ai.ahead)
					headOrient = -ai.angle;
					SpawnDragonSmokeBreathEffect(item, DragonMouthBite);

				if (isTargetAhead)
				{
					item.Animation.TargetState = DRAGON_STATE_FIRE_1;
					creature.Flags = 30;
				}
				else
				{
					item.Animation.TargetState = DRAGON_STATE_AIM_1;
					creature.Flags = 0;
				}

				break;

			case DRAGON_STATE_FIRE_1:
				item.Pose.Orientation.y -= headingAngle;
				SoundEffect(SFX_TR2_DRAGON_FIRE, &item.Pose);

				if (ai.ahead)
				{
					headOrient = -ai.angle;
					SpawnDragonSmokeBreathEffect(item, DragonMouthBite);
				}

				if (creature.Flags)
				{
					if (ai.ahead)
					{
						SpawnDragonFireBreathEffect(item, DragonMouthBite);
						SpawnDragonSmokeBreathEffect(item, DragonMouthBite);
					}

					creature.Flags--;
				}
				else
				{
					item.Animation.TargetState = DRAGON_STATE_IDLE;
				}

				break;
			}
		}

		if (timer >= 0)
		{
			CreatureJoint(&item, 0, headOrient);
			CreatureAnimation(itemNumber, headingAngle, 0);
		}

		SyncDragonBackSegment(item);
	}

	static void HandleDaggerPickup(ItemInfo& item, ItemInfo& playerItem)
	{
		auto& player = GetLaraInfo(playerItem);

		if ((IsHeld(In::Action) &&
			(item.Animation.AnimNumber == DRAGON_ANIM_DEFEATED ||
				(item.Animation.AnimNumber == DRAGON_ANIM_RECOVER && item.Animation.FrameNumber <= DRAGON_ALMOST_LIVE)) &&
			playerItem.Animation.ActiveState == LS_IDLE &&
			playerItem.Animation.AnimNumber == LA_STAND_IDLE &&
			player.Control.HandStatus == HandStatus::Free) ||
			player.Control.IsMoving && player.Context.InteractedItem == item.Index)
		{
			auto bounds = GameBoundingBox(&item);

			DragonDaggerBounds.BoundingBox.X1 = bounds.X1 - BLOCK(1.0f);
			DragonDaggerBounds.BoundingBox.X2 = bounds.X2 + BLOCK(1.0f);
			DragonDaggerBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(1.0f);
			DragonDaggerBounds.BoundingBox.Z2 = bounds.Z2 + BLOCK(1.0f);

			DragonDaggerPos.x = bounds.X2 - BLOCK(2.5f);
			DragonDaggerPos.z = bounds.Z1 + BLOCK(0.9f);

			if (TestLaraPosition(DragonDaggerBounds, &item, &playerItem))
			{
				// HACK: Temporarily change orientation.
				short yOrient = item.Pose.Orientation.y;
				item.Pose.Orientation.y += ANGLE(90.0f);

				if (MoveLaraPosition(DragonDaggerPos, &item, &playerItem))
				{
					// TODO: Reimplement dagger pickup animation when state transitions
					// from ID_LARA_EXTRA_ANIMS to ID_LARA are possible. -- Adngel 2023.10.03

					//SetAnimation(*playerItem, ID_LARA_EXTRA_ANIMS, LEA_PULL_DAGGER_FROM_DRAGON);
					//playerItem.Pose = item.Pose;

					// HACK: Temporarily use small button push animation.
					SetAnimation(playerItem, LA_PICKUP_PEDESTAL_LOW);

					ResetPlayerFlex(&playerItem);
					playerItem.Animation.FrameNumber = 0;
					player.Control.IsMoving = false;
					player.Control.HandStatus = HandStatus::Busy;

					AnimateItem(playerItem);

					// Setting ItemFlags[1] to negative value sets defeat status and triggers death.
					item.ItemFlags[1] = -1 * (100 - GetAnimData(playerItem).EndFrameNumber);
				}
				else
				{
					player.Context.InteractedItem = item.Index;
				}

				// Restore orientation.
				item.Pose.Orientation.y = yOrient;
			}
			else if (player.Control.IsMoving && player.Context.InteractedItem == item.Index)
			{
				player.Control.IsMoving = false;
				player.Control.HandStatus = HandStatus::Free;
			}
		}
	}

	void CollideDragonFront(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];
		const auto& player = *GetLaraInfo(playerItem);

		if (item.Animation.ActiveState == DRAGON_STATE_DEFEAT &&
			item.TriggerFlags == DRAGON_OCB_DAGGER)
		{
			HandleDaggerPickup(item, *playerItem);

			if (player.Control.IsMoving &&
				player.Context.InteractedItem == item.Index ||
				playerItem->Animation.AnimNumber == LA_BUTTON_SMALL_PUSH)
			{
				return;
			}
			else
			{
				CreatureCollision(itemNumber, playerItem, coll);
			}
		}
		else
		{
			if (item.HitPoints > 0)
				CreatureCollision(itemNumber, playerItem, coll);
		}
	}

	void CollideDragonBack(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		const auto& item = g_Level.Items[itemNumber];

		if (item.HitPoints > 0)
			CreatureCollision(itemNumber, playerItem, coll);
	}
}
