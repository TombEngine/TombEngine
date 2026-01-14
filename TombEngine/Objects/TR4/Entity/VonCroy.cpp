#include "framework.h"
#include "Objects/TR4/Entity/VonCroy.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	constexpr auto VON_CROY_FLAG_JUMP = 6;
	constexpr auto VON_CROY_JUMP_RANGE = BLOCK(0.79f);
	constexpr auto VON_CROY_WALK_TURN_RATE = ANGLE(7.0f);
	constexpr auto VON_CROY_RUN_TURN_RATE = ANGLE(11.0f);
	constexpr auto VON_CROY_ATTACK_RANGE = BLOCK(0.8f);
	constexpr auto VON_CROY_RUN_RANGE = BLOCK(3.0f);
	constexpr auto VON_CROY_VAULT_SHIFT = 260;
	constexpr auto VON_CROY_BOOK_EQUIP_FRAME = 26;
	constexpr auto VON_CROY_BOOK_UNEQUIP_FRAME = 224;
	constexpr auto VON_CROY_EQUIP_UNEQUIP_FRAME = 28;
	constexpr auto VON_CROY_ADJUST_POSITION_TURN_RATE = ANGLE(2.81f);
	constexpr auto VON_CROY_ADJUST_POSITION_VELOCITY = 15;
	constexpr auto VON_CROY_AI_PATH_DETECTION_RADIUS = BLOCK(0.65f);
	constexpr auto VON_CROY_CALL_LARA_RANGE = BLOCK(5.0f);
	constexpr auto VON_CROY_CALL_TIMER = 40;

	constexpr auto VON_CROY_TORSO_MESHINDEX = 7;
	constexpr auto VON_CROY_HAND_LEFT_MESHINDEX = 15;
	constexpr auto VON_CROY_HAND_RIGHT_MESHINDEX = 18;

	const auto VonCroyBite = CreatureBiteInfo(Vector3(0.0f, 35.0f, 130.0f), VON_CROY_HAND_RIGHT_MESHINDEX);
	const auto VonCroyBookSwapJoints = std::vector<unsigned int>{ VON_CROY_HAND_LEFT_MESHINDEX };
	const auto VonCroyKnifeSwapJoints = std::vector<unsigned int>{ VON_CROY_TORSO_MESHINDEX, VON_CROY_HAND_RIGHT_MESHINDEX };

	enum VonCroyState
	{
		// No state 0.
		VON_CROY_STATE_IDLE = 1,
		VON_CROY_STATE_WALK = 2,
		VON_CROY_STATE_RUN = 3,
		VON_CROY_STATE_START_MONKEY = 4,
		VON_CROY_STATE_MONKEY = 5,
		VON_CROY_STATE_EQUIP_UNEQUIP_KNIFE = 6,
		VON_CROY_STATE_LOOK_BEFORE_JUMP = 7,
		VON_CROY_STATE_TALK_1 = 8,
		VON_CROY_STATE_TALK_2 = 9,
		VON_CROY_STATE_TALK_3 = 10,
		VON_CROY_STATE_TALK_WITH_BOOK = 11,
		VON_CROY_STATE_STOP_LARA = 12,
		VON_CROY_STATE_CALL_LARA_1 = 13,
		VON_CROY_STATE_CALL_LARA_2 = 14,
		VON_CROY_STATE_JUMP_1_BLOCK = 15,
		VON_CROY_STATE_JUMP_2_BLOCKS = 16,
		VON_CROY_STATE_CLIMB_4_CLICKS = 17,
		VON_CROY_STATE_CLIMB_3_CLICKS = 18,
		VON_CROY_STATE_CLIMB_2_CLICKS = 19,
		VON_CROY_STATE_ENABLE_TRAP = 20,
		VON_CROY_STATE_KNIFE_ATTACK_HIGH = 21,
		VON_CROY_STATE_LOOK_BACK_LEFT = 22,
		VON_CROY_STATE_JUMP_DOWN_2_CLICKS = 23,
		VON_CROY_STATE_JUMP_DOWN_3_CLICKS = 24,
		VON_CROY_STATE_JUMP_DOWN_4_CLICKS = 25,
		VON_CROY_STATE_JUMP_DOWN_7_CLICKS = 26,
		VON_CROY_STATE_GRAB_LADDER = 27,
		VON_CROY_STATE_CLIMB_LADDER_RIGHT = 28,
		VON_CROY_STATE_NONE = 29,
		VON_CROY_STATE_LADDER_CLIMB_UP = 30,
		VON_CROY_STATE_KNIFE_ATTACK_LOW = 31,
		VON_CROY_STATE_POINT = 32,
		VON_CROY_STATE_STANDING_JUMP_GRAB = 33,
		VON_CROY_STATE_JUMP_BACK = 34,
		VON_CROY_STATE_LOOK_BACK_RIGHT = 35,
		VON_CROY_STATE_POSITION_ADJUST_FRONT = 36,
		VON_CROY_STATE_POSITION_ADJUST_BACK = 37
	};

	enum VonCroyAnim
	{
		VON_CROY_ANIM_WALK_FORWARD = 0,
		VON_CROY_ANIM_RUN_FORWARD = 1,
		VON_CROY_ANIM_MONKEY_FORWARD = 2,
		VON_CROY_ANIM_USE_SWITCH = 3,
		VON_CROY_ANIM_IDLE = 4,
		VON_CROY_ANIM_ATTACK_HIGH = 5,
		VON_CROY_ANIM_MONKEY_TO_FORWARD = 6,
		VON_CROY_ANIM_MONKEY_IDLE = 7,
		VON_CROY_ANIM_MONKEY_IDLE_TO_FORWARD = 8,
		VON_CROY_ANIM_MONKEY_STOP = 9,
		VON_CROY_ANIM_MONKEY_FALL = 10,
		VON_CROY_ANIM_KNIFE_EQUIP_UNEQUIP = 11,
		VON_CROY_ANIM_GROUND_INSPECT = 12,
		VON_CROY_ANIM_IDLE_TO_WALK = 13,
		VON_CROY_ANIM_WALK_STOP = 14,
		VON_CROY_ANIM_IDLE_TO_RUN = 15,
		VON_CROY_ANIM_RUN_STOP = 16,
		VON_CROY_ANIM_WALK_TO_RUN = 17,
		VON_CROY_ANIM_RUN_TO_WALK = 18,
		VON_CROY_ANIM_TALKING1 = 19,
		VON_CROY_ANIM_TALKING2 = 20,
		VON_CROY_ANIM_TALKING3 = 21,
		VON_CROY_ANIM_IDLE_TO_JUMP = 22,
		VON_CROY_ANIM_JUMP_1_BLOCK = 23,
		VON_CROY_ANIM_JUMP_LAND = 24,
		VON_CROY_ANIM_JUMP_2_BLOCKS = 25,
		VON_CROY_ANIM_LEFT_TURN = 26,
		VON_CROY_ANIM_CLIMB_4_CLICKS = 27,
		VON_CROY_ANIM_CLIMB_3_CLICKS = 28,
		VON_CROY_ANIM_CLIMB_2_CLICKS = 29,
		VON_CROY_ANIM_IDLE_TALKING4_WITH_BOOK = 30,
		VON_CROY_ANIM_LARA_INTERACT_COME_DISTANT = 31,
		VON_CROY_ANIM_LARA_INTERACT_COME_CLOSE = 32,
		VON_CROY_ANIM_LARA_INTERACT_STOP = 33,
		VON_CROY_ANIM_LARA_INTERACT_STOP_TO_COME = 34,
		VON_CROY_ANIM_CLIMB_DOWN_4_CLICKS = 35,
		VON_CROY_ANIM_CLIMB_DOWN_7_CLICKS = 36,
		VON_CROY_ANIM_JUMP_TO_HANG = 37,
		VON_CROY_ANIM_SHIMMY_TO_THE_RIGHT = 38,
		VON_CROY_ANIM_CLIMB = 39,
		VON_CROY_ANIM_DROP_FROM_HANG = 40,
		VON_CROY_ANIM_CLIMB_OFF_3_CLICKS = 41,
		VON_CROY_ANIM_CLIMB_OFF_2_CLICKS = 42,
		VON_CROY_ANIM_HANG = 43,
		VON_CROY_ANIM_ATTACK_LOW = 44,
		VON_CROY_ANIM_RUNNING_JUMP_RIGHT_FOOT = 45,
		VON_CROY_ANIM_RUNNING_JUMP_LEFT_FOOT = 46,
		VON_CROY_ANIM_START_POINT = 47,
		VON_CROY_ANIM_POINTING = 48,
		VON_CROY_ANIM_STOP_POINTING = 49,
		VON_CROY_ANIM_RUNNING_JUMP = 50,
		VON_CROY_ANIM_RUNNING_JUMP_TO_GRAB = 51,
		VON_CROY_ANIM_CLIMB_UP_AFTER_JUMP = 52,
		VON_CROY_ANIM_STANDING_JUMP_BACK_START = 53,
		VON_CROY_ANIM_STANDING_JUMP_BACK = 54,
		VON_CROY_ANIM_STANDING_JUMP_BACK_END = 55,
		VON_CROY_ANIM_TURN_TO_THE_RIGHT = 56,
		VON_CROY_ANIM_ALIGN_FRONT = 57,
		VON_CROY_ANIM_ALIGN_BACK = 58,
		VON_CROY_ANIM_LAND_TO_RUN = 59
	};

	const auto VonCroyTalkStateList = std::vector<int>{ VON_CROY_STATE_TALK_1, VON_CROY_STATE_TALK_2, VON_CROY_STATE_TALK_3 };

	static void DoKnifeMeshSwap(ItemInfo& item, bool forceEquip = false)
	{
		item.SetMeshSwapFlags(VonCroyKnifeSwapJoints, forceEquip || item.TestMeshSwapFlags(VonCroyKnifeSwapJoints));
	}

	static void DoBookMeshSwap(ItemInfo& item)
	{
		item.SetMeshSwapFlags(VonCroyBookSwapJoints, item.TestMeshSwapFlags(VonCroyBookSwapJoints));
	}

	static void DoNodePath(ItemInfo& item, CreatureInfo& creature)
	{
		// Use it to setup the path
		FindAITargetObject(&creature, ID_AI_FOLLOW, creature.LocationAI, false);
		creature.ReachedGoal = Vector3i::Distance(item.Pose.Position, creature.Enemy->Pose.Position) <= VON_CROY_AI_PATH_DETECTION_RADIUS;
	}

	static void RotateTowardTarget(ItemInfo& item, const short angle, short turnRate)
	{
		if (abs(angle) < turnRate)
		{
			item.Pose.Orientation.y += angle;
		}
		else if (angle < 0)
		{
			item.Pose.Orientation.y -= turnRate;
		}
		else
		{
			item.Pose.Orientation.y += turnRate;
		}
	}

	static void GetRotationTowardPlayer(ItemInfo& item, ItemInfo& target, short& angle, short& xAngle)
	{
		const auto& object = Objects[item.ObjectNumber];
		const auto& lara = GetLaraInfo(target);
		int x = target.Pose.Position.x + (PREDICTIVE_SCALE_FACTOR * target.Animation.Velocity.z * phd_sin(lara.Control.MoveAngle)) - (object.pivotLength * phd_sin(item.Pose.Orientation.y)) - item.Pose.Position.x;
		int y = target.Pose.Position.y - item.Pose.Position.y;
		int z = target.Pose.Position.z + (PREDICTIVE_SCALE_FACTOR * target.Animation.Velocity.z * phd_cos(lara.Control.MoveAngle)) - (object.pivotLength * phd_cos(item.Pose.Orientation.y)) - item.Pose.Position.z;
		angle = phd_atan(z, x) - item.Pose.Orientation.y;
		x = abs(x);
		z = abs(z);
		if (x > z)
			xAngle = -phd_atan(x + (z >> 1), y);
		else
			xAngle = -phd_atan(z + (x >> 1), y);
	}

	static bool IsPlayerNear(ItemInfo& item, ItemInfo& target, float range)
	{
		return Vector3i::Distance(item.Pose.Position, target.Pose.Position) <= range;
	}

	static bool IsVonCroyTalking(ItemInfo& item)
	{
		return item.Animation.ActiveState == VON_CROY_STATE_TALK_1 ||
			   item.Animation.ActiveState == VON_CROY_STATE_TALK_2 ||
			   item.Animation.ActiveState == VON_CROY_STATE_TALK_3 ||
			   item.Animation.ActiveState == VON_CROY_STATE_TALK_WITH_BOOK;
	}

	void InitializeVonCroy(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);
		SetAnimation(item, VON_CROY_ANIM_KNIFE_EQUIP_UNEQUIP);
		DoKnifeMeshSwap(item);
		item.ItemFlags[1] = 0; // 0= Use AI_Path or 1= Follow player.
		item.ItemFlags[2] = 0; // 1= Call for player. 2= During cutscene.
	}

	void VonCroyControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& creature = *GetCreatureInfo(&item);
		short angle = 0;
		short tilt = 0;
		ItemInfo* enemy = nullptr;
		EulerAngles head = {};
		EulerAngles torso = {};
		AI_INFO ai = {};

		// Check whether Von Croy can jump 1, 2 or 3 blocks.
		bool canJump1block  = CanCreatureJump(item, JumpDistance::Block1, VON_CROY_JUMP_RANGE);
		bool canJump2blocks = CanCreatureJump(item, JumpDistance::Block2, VON_CROY_JUMP_RANGE);
		bool canJump3blocks = CanCreatureJump(item, JumpDistance::Block3, VON_CROY_JUMP_RANGE);

		GetAITarget(&creature);
		CreatureAIInfo(&item, &ai);

		if (item.ItemFlags[1] == 0)
		{
			DoNodePath(item, creature);
			if (creature.ReachedGoal)
			{
				switch (item.ItemFlags[2])
				{
				case 1: // Call lara.
					GetRotationTowardPlayer(item, *LaraItem, torso.y, torso.x);
					RotateTowardTarget(item, ai.angle, VON_CROY_ADJUST_POSITION_TURN_RATE);
					if (item.Timer == 0)
					{
						if (IsPlayerNear(item, *LaraItem, VON_CROY_CALL_LARA_RANGE))
							item.Animation.TargetState = VON_CROY_STATE_CALL_LARA_2;
						else
							item.Animation.TargetState = VON_CROY_STATE_CALL_LARA_1;
						item.Timer = VON_CROY_CALL_TIMER;
					}
					else
					{
						item.Timer--;
						if (item.Timer < 0)
							item.Timer = 0;
					}
					break;
				case 3: // Currently in cinematic, normal talk.
					if (IsVonCroyTalking(item))
						break;

					item.Animation.TargetState = VonCroyTalkStateList[Random::GenerateInt(0, 2)];
					break;
				case 4: // Currently in cinematic, look back left/right
					enemy = creature.Enemy;
					if (enemy->Pose.Orientation.y - item.Pose.Orientation.y < -1024)
						item.Animation.TargetState = VON_CROY_STATE_LOOK_BACK_RIGHT;
					else if (enemy->Pose.Orientation.y - item.Pose.Orientation.y > 1024)
						item.Animation.TargetState = VON_CROY_STATE_LOOK_BACK_LEFT;
					break;
				case 5:
					// Now talk, equip book then when finished, unequip it !
					if (item.Animation.FrameNumber == VON_CROY_BOOK_EQUIP_FRAME)
					{
						DoBookMeshSwap(item);
					}
					else if (item.Animation.FrameNumber == VON_CROY_BOOK_UNEQUIP_FRAME)
					{
						DoBookMeshSwap(item);
						item.ItemFlags[2] = 3; // Do the normal talk after the book talk is finished.
					}
					break;
				}
			}
		}
		else
		{
			item.AIBits = FOLLOW;
			GetCreatureMood(&item, &ai, true);
			CreatureMood(&item, &ai, true);
			angle = CreatureTurn(&item, creature.MaxTurn);
		}

		enemy = creature.Enemy;
		switch (item.Animation.ActiveState)
		{
		case VON_CROY_STATE_IDLE:
			creature.MaxTurn = 0;
			creature.Flags = 0;
			creature.LOT.IsMonkeying = false;
			creature.LOT.IsJumping = false;

			if (creature.ReachedGoal && item.ItemFlags[2] > 0)
				break;

			if (ai.ahead)
			{
				head.y = ai.angle;
				head.x = ai.xAngle;
			}

			if (item.Animation.RequiredState != NO_VALUE)
			{
				item.Animation.TargetState = item.Animation.RequiredState;
			}
			else if (Vector3i::Distance(item.Pose.Position, enemy->Pose.Position) <= VON_CROY_ATTACK_RANGE)
			{
				if (creature.ReachedGoal && enemy->ObjectNumber == ID_AI_FOLLOW)
				{
					if (ai.ahead)
						item.Animation.TargetState = VON_CROY_STATE_POSITION_ADJUST_FRONT;
					else
						item.Animation.TargetState = VON_CROY_STATE_POSITION_ADJUST_BACK;
				}
				else
				{
					GameBoundingBox box(enemy);
					if (box.GetHeight() <= CLICK(2.5f))
						item.Animation.TargetState = VON_CROY_STATE_KNIFE_ATTACK_LOW;
					else
						item.Animation.TargetState = VON_CROY_STATE_KNIFE_ATTACK_HIGH;
				}
			}
			else if (Vector3i::Distance(item.Pose.Position, enemy->Pose.Position) > VON_CROY_RUN_RANGE)
			{
				item.Animation.TargetState = VON_CROY_STATE_RUN;
			}
			else
			{
				item.Animation.TargetState = VON_CROY_STATE_WALK;
			}

			break;

		case VON_CROY_STATE_WALK:
			creature.MaxTurn = VON_CROY_WALK_TURN_RATE;
			if (item.ItemFlags[1] == 0 && !creature.ReachedGoal)
				RotateTowardTarget(item, ai.angle, creature.MaxTurn);

			if (Vector3i::Distance(item.Pose.Position, enemy->Pose.Position) <= VON_CROY_ATTACK_RANGE)
			{
				item.Animation.TargetState = VON_CROY_STATE_IDLE;
			}

			break;

		case VON_CROY_STATE_RUN:
			creature.MaxTurn = VON_CROY_RUN_TURN_RATE;
			if (item.ItemFlags[1] == 0 && !creature.ReachedGoal)
				RotateTowardTarget(item, ai.angle, creature.MaxTurn);

			if (Vector3i::Distance(item.Pose.Position, enemy->Pose.Position) <= VON_CROY_RUN_RANGE)
			{
				item.Animation.TargetState = VON_CROY_STATE_WALK;
			}

			break;

		case VON_CROY_STATE_EQUIP_UNEQUIP_KNIFE:
			if (item.Animation.FrameNumber == VON_CROY_EQUIP_UNEQUIP_FRAME)
			{
				DoKnifeMeshSwap(item);
			}
			break;

		case VON_CROY_STATE_POSITION_ADJUST_FRONT:
		case VON_CROY_STATE_POSITION_ADJUST_BACK:
			creature.MaxTurn = 0;
			if (MoveCreature3DPos(&item.Pose, &enemy->Pose, VON_CROY_ADJUST_POSITION_VELOCITY, enemy->Pose.Orientation.y - item.Pose.Orientation.y, VON_CROY_ADJUST_POSITION_TURN_RATE))
			{
				item.Animation.TargetState = VON_CROY_STATE_IDLE;
				item.ItemFlags[2] = 1; // Next voncroy state.
			}
			break;
		}

		CreatureTilt(&item, tilt);
		CreatureJoint(&item, 0, torso.y);
		CreatureJoint(&item, 1, torso.x);
		CreatureJoint(&item, 2, head.y);
		CreatureJoint(&item, 3, head.x);

		if (item.Animation.ActiveState < VON_CROY_STATE_JUMP_1_BLOCK &&
			item.Animation.ActiveState != VON_CROY_STATE_MONKEY)
		{
			switch (CreatureVault(itemNumber, angle, 2, VON_CROY_VAULT_SHIFT))
			{
			case 2:
				creature.MaxTurn = 0;
				SetAnimation(item, VON_CROY_ANIM_CLIMB_2_CLICKS);
				break;

			case 3:
				creature.MaxTurn = 0;
				SetAnimation(item, VON_CROY_ANIM_CLIMB_3_CLICKS);
				break;

			case 4:
				creature.MaxTurn = 0;
				SetAnimation(item, VON_CROY_ANIM_CLIMB_4_CLICKS);
				break;

			case 7:
				creature.MaxTurn = 0;
				SetAnimation(item, VON_CROY_ANIM_JUMP_TO_HANG);
				break;

			case -2:
				creature.MaxTurn = 0;
				SetAnimation(item, VON_CROY_ANIM_CLIMB_OFF_2_CLICKS);
				break;

			case -3:
				creature.MaxTurn = 0;
				SetAnimation(item, VON_CROY_ANIM_CLIMB_OFF_3_CLICKS);
				break;

			case -4:
				creature.MaxTurn = 0;
				SetAnimation(item, VON_CROY_ANIM_CLIMB_DOWN_4_CLICKS);
				break;

			case -7:
				creature.MaxTurn = 0;
				SetAnimation(item, VON_CROY_ANIM_CLIMB_DOWN_7_CLICKS);
				break;
			}
		}
		else
		{
			CreatureAnimation(itemNumber, angle, tilt);
		}
	}
}
