#include "framework.h"
#include "Willard.h"

#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/gui.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Objects/Effects/Boss.h"
#include "Math/Math.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Gui;
using namespace TEN::Effects::Boss;

/// item.ItemFlags[0] = Boss flags.
/// item.ItemFlags[1] = Quest 1 Object Number.
/// item.ItemFlags[2] = Quest 2 Object Number.
/// item.ItemFlags[3] = Quest 3 Object Number.
/// item.ItemFlags[4] = Quest 4 Object Number.
/// item.ItemFlags[5] = AI_X1 count.
/// item.ItemFlags[6] = Willard direction.
/// item.ItemFlags[7] = Willard behaviour.

namespace TEN::Entities::Creatures::TR3
{
	enum WillardAnim
	{
		WILLARD_ANIM_STOP = 0,
		WILLARD_ANIM_WALK_START = 1,
		WILLARD_ANIM_WALK = 2,
		WILLARD_ANIM_WALK_TO_STOP_LEFT = 3,
		WILLARD_ANIM_WALK_TO_STOP_RIGHT = 4,
		WILLARD_ANIM_STOP_ATTACK = 5,
		WILLARD_ANIM_KILL_LARA = 6,
		WILLARD_ANIM_STUN_START = 7,
		WILLARD_ANIM_STUN = 8,
		WILLARD_ANIM_STUN_TO_STOP = 9,
		WILLARD_ANIM_WALK_ATTACK_RIGHT = 10,
		WILLARD_ANIM_WALK_ATTACK_LEFT = 11,
		WILLARD_ANIM_SHOOT = 12
	};

	enum WillardState
	{
		WILLARD_STATE_STOP = 0,
		WILLARD_STATE_WALK = 1,
		WILLARD_STATE_ATTACK = 2,
		WILLARD_STATE_KILL_LARA = 3,
		WILLARD_STATE_STUN_START = 4,
		WILLARD_STATE_STUN = 5,
		WILLARD_STATE_STUN_END = 6,
		WILLARD_STATE_WALK_ATTACK_RIGHT = 7,
		WILLARD_STATE_WALK_ATTACK_LEFT = 8,
		WILLARD_STATE_JUMP_TURN_180 = 9,
		WILLARD_STATE_SHOOT = 10
	};


	constexpr auto WILLARD_AI_X1_RANGE_DETECTION = BLOCK(1.0f);
	constexpr auto WILLARD_AI_X1_PLAYER_RANGE_DETECTION = BLOCK(1.25f);
	constexpr auto WILLARD_AI_X2_RANGE_DETECTION = BLOCK(1.0f);
	constexpr auto WILLARD_AI_GUARD_PLAYER_RANGE_DETECTION = BLOCK(5.0f); // Does the player is near a guard AI ?
	constexpr auto WILLARD_WALK_TURN_RATE_MAX = ANGLE(8.0f);
	constexpr auto WILLARD_STOP_ATTACK_RANGE = BLOCK(1.5f);
	constexpr auto WILLARD_WALK_ATTACK_RANGE = BLOCK(3.0f);

	constexpr auto WILLARD_COLLIDE_DAMAGE = 10;

	constexpr auto WILLARD_HEAD_MESH_INDEX = 17;
	constexpr auto WILLARD_HAND_LEFT_INDEX = 20;
	constexpr auto WILLARD_HAND_RIGHT_INDEX = 23;

	const auto WillardBiteHead = CreatureBiteInfo(Vector3(0.0f, -16.0f, 16.0f), WILLARD_HEAD_MESH_INDEX); // For plasma ball.
	const auto WillardBiteLeft = CreatureBiteInfo(Vector3(19.0f, -13.0f, 3.0f), WILLARD_HAND_LEFT_INDEX);
	const auto WillardBiteRight = CreatureBiteInfo(Vector3(19.0f, -13.0f, 3.0f), WILLARD_HAND_RIGHT_INDEX);

	static bool IsPickupInInventory(const ItemInfo& item, int index)
	{
		return g_Gui.IsObjectInInventory(item.ItemFlags[index]) != 0;
	}

	static bool IsAllPickupInInventory(const ItemInfo& item)
	{
		return IsPickupInInventory(item, 0) &&
			   IsPickupInInventory(item, 1) &&
			   IsPickupInInventory(item, 2) &&
			   IsPickupInInventory(item, 3);
	}

	static int GetNodePathCount(GAME_OBJECT_ID objectNumber)
	{
		int count = 0;
		for (auto& aiObj : g_Level.AIObjects)
		{
			if (aiObj.objectNumber == objectNumber)
				count++;
		}
		return count;
	}

	static int GetNodePathCount(ItemInfo& item, ItemInfo& target, int currentNode, GAME_OBJECT_ID objectNumber, int forwardOrBackward)
	{
		// First get all nodes
		std::vector<const AI_OBJECT*> nodeList = {};
		for (auto& aiObj : g_Level.AIObjects)
		{
			if (aiObj.objectNumber == objectNumber)
				nodeList.push_back(&aiObj);
		}

		// Now sort it based on OCB.
		std::sort(nodeList.begin(), nodeList.end(), [&](const AI_OBJECT* a, const AI_OBJECT* b)
		{
				// Forward
				// Should be 0, 1, 2, 3, 4, X
				if (forwardOrBackward == 0)
					return a->triggerFlags < b->triggerFlags;
				// Backward
				// Should be X, 4, 3, 2, 1, 0
				else return a->triggerFlags > b->triggerFlags;
		});

		// Now calculate the closest to player based on triggerFlags and distance.
		int nodeCountUntilFound = 0;
		for (int i = 0; i < (int)nodeList.size(); i++, nodeCountUntilFound++)
		{
			if (Vector3i::Distance(target.Pose.Position, nodeList[i]->pos.Position) <= WILLARD_AI_X1_PLAYER_RANGE_DETECTION)
				return nodeCountUntilFound;
		}

		// Nothing found ?
		return NO_VALUE;
	}

	static int GetWillardMoveDirection(ItemInfo& item, CreatureInfo& creature, ItemInfo& target)
	{
		// Check forward path
		int forwardCount = GetNodePathCount(item, target, creature.LocationAI, ID_AI_X1, 0);
		// Check backward path
		int backwardCount = GetNodePathCount(item, target, creature.LocationAI, ID_AI_X1, 1);

		DrawDebugString(std::string("Forward: " + std::to_string(forwardCount) + ", Backward: " + std::to_string(backwardCount)), Vector2(150, 370), Color(1, 1, 0), 1.0f, RendererDebugPage::WireframeMode);

		// Check if lara is near forward or backward
		// Then change direction based on what happen.
		if (forwardCount != backwardCount)
		{
			// If true, then player is near forward path, continue forward.
			if (forwardCount < backwardCount)
				return 0;
			// Switch direction to backward and turn 180.
			else
				return 1;
		}

		return 0;
	}

	static void DoNodePath(ItemInfo& item, CreatureInfo& creature, ItemInfo& target)
	{
		// Use it to setup the path
		FindAITargetObject(&creature, ID_AI_X1, creature.LocationAI, false);
		int direction = GetWillardMoveDirection(item, creature, target);
		if (Vector3i::Distance(item.Pose.Position, creature.Enemy->Pose.Position) <= WILLARD_AI_X1_RANGE_DETECTION)
		{
			switch (direction)
			{
			case 0: // Forward
				creature.LocationAI++;
				if (creature.LocationAI > item.ItemFlags[5])
					creature.LocationAI = 0;
				if (item.ItemFlags[6] == 1)
				{
					item.Animation.TargetState = WILLARD_STATE_STOP;
					item.Animation.RequiredState = WILLARD_STATE_JUMP_TURN_180;
					item.ItemFlags[6] = 0;
				}
				break;
			case 1: // Backward
				creature.LocationAI--;
				if (creature.LocationAI < 0)
					creature.LocationAI = item.ItemFlags[5];
				if (item.ItemFlags[6] == 0)
				{
					item.Animation.TargetState = WILLARD_STATE_STOP;
					item.Animation.RequiredState = WILLARD_STATE_JUMP_TURN_180;
					item.ItemFlags[6] = 1;
				}
				break;
			}
		}

		// Use it to get behaviour if you arrive on X2 ai without modifing the creature->Enemy.
		AITargetFlags data = {};
		data.checkDistance = true;
		data.checkOcb = false;
		data.objectNumber = ID_AI_X2;
		data.ocb = NO_VALUE;
		data.checkSameZone = false;
		data.maxDistance = WILLARD_AI_X2_RANGE_DETECTION;
		if (FindAITargetObject(&creature, &data))
		{
			DrawDebugSphere(data.foundItem.Pose.Position.ToVector3(), 128.0f, Color(1, 1, 0), RendererDebugPage::WireframeMode, true);
			item.ItemFlags[1] = data.foundItem.TriggerFlags;
		}
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

	static short RotateTowardPlayer(ItemInfo& item, ItemInfo& target)
	{
		const auto& object = Objects[item.ObjectNumber];
		const auto& lara = GetLaraInfo(target);
		int x = target.Pose.Position.x + (PREDICTIVE_SCALE_FACTOR * target.Animation.Velocity.z * phd_sin(lara.Control.MoveAngle)) - (object.pivotLength * phd_sin(item.Pose.Orientation.y)) - item.Pose.Position.x;
		int z = target.Pose.Position.z + (PREDICTIVE_SCALE_FACTOR * target.Animation.Velocity.z * phd_cos(lara.Control.MoveAngle)) - (object.pivotLength * phd_cos(item.Pose.Orientation.y)) - item.Pose.Position.z;
		return phd_atan(z, x) - item.Pose.Orientation.y;
	}

	void InitializeWillard(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);

		// Keep object number so we can see if any pickup did get picked up by player !
		// Also doing this by ItemFlags[] so it can be customized.
		item.ItemFlags[1] = ID_PICKUP_ITEM3;
		item.ItemFlags[2] = ID_PICKUP_ITEM4;
		item.ItemFlags[3] = ID_PICKUP_ITEM5;
		item.ItemFlags[4] = ID_PICKUP_ITEM6;
		item.ItemFlags[5] = NO_VALUE; // Store node count.
		item.ItemFlags[6] = 0; // Willard path direction, 1 mean inverted.
		item.ItemFlags[7] = NO_VALUE; // Willard behaviour.

		CheckForRequiredObjects(item);
		SetAnimation(item, WILLARD_ANIM_STOP); // Just to be sure...
	}

	void ControlWillard(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto ai = AI_INFO{};
		auto& item = g_Level.Items[itemNumber];
		auto& object = Objects[item.ObjectNumber];
		auto& creature = GetCreatureInfoRef(item);
		// Required since creature.Enemy will be taken for pathfinding.
		auto& playerItem = *LaraItem;
		auto isPlayerAlive = playerItem.HitPoints > 0;
		short angle = 0;

		// Store node count for making path correct.
		if (item.ItemFlags[5] == NO_VALUE)
			item.ItemFlags[5] = GetNodePathCount(ID_AI_X1) - 1; // Store max AI_X1 to know where to clamp value, -1 because where starting from 0.

		if (item.HitPoints <= 0)
		{
			
		}
		else
		{
			DoNodePath(item, creature, playerItem);
			CreatureAIInfo(&item, &ai);

			if (Vector3i::Distance(item.Pose.Position, playerItem.Pose.Position) <= WILLARD_WALK_ATTACK_RANGE)
				RotateTowardPlayer(item, playerItem);
			else
				RotateTowardTarget(item, ai.angle, WILLARD_WALK_TURN_RATE_MAX);

			// Deal damage to player if willard collide it !
			if (item.TouchBits.TestAny())
				DoDamage(&playerItem, WILLARD_COLLIDE_DAMAGE);

			switch (item.Animation.ActiveState)
			{
			case WILLARD_STATE_STOP:
				creature.Flags = 0; // Reset attack cooldown.

				if (item.Animation.RequiredState != NO_VALUE)
				{
					item.Animation.TargetState = item.Animation.RequiredState;
				}
				else if (Vector3i::Distance(item.Pose.Position, playerItem.Pose.Position) <= WILLARD_STOP_ATTACK_RANGE)
				{
					item.Animation.TargetState = WILLARD_STATE_ATTACK;
				}
				else
				{
					item.Animation.TargetState = WILLARD_STATE_WALK;
				}

				break;

			case WILLARD_STATE_WALK:
				creature.Flags = 0; // Reset attack cooldown.

				if (Vector3i::Distance(item.Pose.Position, playerItem.Pose.Position) <= WILLARD_WALK_ATTACK_RANGE)
				{
					// Check if we can stop and do stop attack
					if (Random::TestProbability(0.85f))
					{
						item.Animation.TargetState = WILLARD_STATE_STOP;
						break;
					}

					// if true, right attack
					if (Random::TestProbability(0.5f))
						item.Animation.TargetState = WILLARD_STATE_WALK_ATTACK_RIGHT;
					else
						item.Animation.TargetState = WILLARD_STATE_WALK_ATTACK_LEFT;
				}
				break;

			case WILLARD_STATE_JUMP_TURN_180:
				const auto& anim = GetAnimData(item.Animation.AnimNumber);
				if (item.Animation.FrameNumber >= anim.frameEnd-1)
					item.Pose.Orientation.y += ANGLE(180.0f);
				break;
			}
		}

		if (isPlayerAlive && LaraItem->HitPoints <= 0)
		{
			CreatureKill(&item, WILLARD_ANIM_KILL_LARA, LEA_WILLARD_DEATH, WILLARD_STATE_KILL_LARA, LS_DEATH);
			return;
		}

		CreatureAnimation(itemNumber, angle, 0);
	}
}
