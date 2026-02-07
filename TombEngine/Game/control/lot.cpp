/**
 * @file lot.cpp
 * @brief Layout Of Terrain (LOT) system for creature AI pathfinding.
 *
 * The LOT system manages creature navigation capabilities and pathfinding initialization.
 * Each creature has a LOTInfo structure that defines:
 * - Movement capabilities (Step, Drop, Fly)
 * - Zone type for pathfinding (determines which boxes are accessible)
 * - Navigation state (target box, search data)
 *
 * The system works with the box/zone system defined in box.cpp to enable creatures
 * to navigate the level geometry.
 */

#include "framework.h"
#include "Game/control/lot.h"

#include "Game/control/box.h"
#include "Game/camera.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Collision::Room;

// Vertical movement speed for flying creatures (bats, harpies, etc.)
#define DEFAULT_FLY_UPDOWN_SPEED 16

// Vertical movement speed for swimming creatures (sharks, divers, etc.)
// Faster than flying since water provides more resistance/control
#define DEFAULT_SWIM_UPDOWN_SPEED 32

/**
 * @brief Global list of all currently active creature indices.
 *
 * Creatures are added when their AI is enabled (EnableEntityAI) and removed
 * when disabled (DisableEntityAI). Used for creature-vs-creature collision
 * and targeting systems.
 */
std::vector<int> ActiveCreatures;

/**
 * @brief Initializes the pathfinding node array for a creature.
 *
 * Allocates a BoxNode for each pathfinding box in the level. These nodes store
 * the search state during pathfinding (exit box, search number, expansion chain).
 * Only initializes once per creature (checked via LOT.Initialized flag).
 *
 * @param itemNumber Index of the creature item in g_Level.Items.
 */
void InitializeLOTarray(int itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];
	auto* creature = GetCreatureInfo(item);

	if (!creature->LOT.Initialized)
	{
		creature->LOT.Node = std::vector<BoxNode>(g_Level.PathfindingBoxes.size(), BoxNode{});
		creature->LOT.BadBoxes = std::vector<BadBox>(BAD_BOX_MEMORY_SIZE, BadBox{});
		creature->LOT.Initialized = true;
	}
}

/**
 * @brief Enables AI processing for a creature.
 *
 * If the creature doesn't already have AI enabled, initializes its CreatureInfo
 * slot and adds it to the ActiveCreatures list. This must be called before a
 * creature can pathfind or exhibit intelligent behavior.
 *
 * @param itemNum Index of the creature item.
 * @param always Unused parameter (legacy from TR slot system).
 * @param makeTarget If true, creates an AI target item for pathfinding to AI_FOLLOW, etc.
 * @return true if creature AI is now active.
 */
bool EnableEntityAI(short itemNum, bool always, bool makeTarget)
{
	ItemInfo* item = &g_Level.Items[itemNum];

	// Already has AI enabled.
	if (item->IsCreature())
		return true;

	InitializeSlot(itemNum, makeTarget);
	ActiveCreatures.push_back(item->Index);

	return item->IsCreature();
}

/**
 * @brief Disables AI processing for a creature.
 *
 * Cleans up the creature's AI state, destroys its AI target item if any,
 * removes it from the ActiveCreatures list, and clears its CreatureInfo data.
 * Called when a creature dies, is deactivated, or leaves the active area.
 *
 * @param itemNumber Index of the creature item.
 */
void DisableEntityAI(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (!item->IsCreature())
		return;

	auto* creature = GetCreatureInfo(item);
	creature->ItemNumber = NO_VALUE;
	KillItem(creature->AITargetNumber);
	ActiveCreatures.erase(std::find(ActiveCreatures.begin(), ActiveCreatures.end(), item->Index));
	item->Data = nullptr;
}

/**
 * @brief Initializes a creature's AI slot with movement capabilities based on its LotType.
 *
 * This is the core initialization function that sets up a creature's pathfinding parameters.
 * The LotType (defined per object in Objects[]) determines:
 * - Step: Maximum height the creature can step UP to reach another box.
 * - Drop: Maximum height the creature can DROP down to reach another box (negative value).
 * - Fly: Vertical movement speed for flying/swimming creatures (0 = ground-based).
 * - Zone: Which zone array to use for pathfinding connectivity.
 * - CanJump/CanMonkey: Whether the creature can use jump/monkey traversal overlaps.
 *
 * Zone Types:
 * - Basic: Simple ground creatures (1 click step, 2 click drop)
 * - Skeleton: Can jump gaps (uses Skeleton zone)
 * - Human: Can climb 1 block (uses Human zone)
 * - Flyer: Can fly anywhere except water (huge step/drop, uses Flyer zone)
 * - Water: Swims underwater only (huge step/drop, uses Water zone)
 * - Amphibious: Can swim AND walk on land (huge step/drop, uses Amphibious zone)
 *
 * @param itemNumber Index of the creature item.
 * @param makeTarget If true, creates an AI target item for special pathfinding.
 */
void InitializeSlot(short itemNumber, bool makeTarget)
{
	auto* item = &g_Level.Items[itemNumber];
	auto* object = &Objects[item->ObjectNumber];
	item->Data = CreatureInfo();
	auto* creature = GetCreatureInfo(item);

	// Initialize pathfinding node array.
	InitializeLOTarray(itemNumber);

	// Initialize creature state.
	creature->ItemNumber = itemNumber;
	creature->Mood = MoodType::Bored;
	creature->JointRotation[0] = 0;
	creature->JointRotation[1] = 0;
	creature->JointRotation[2] = 0;
	creature->JointRotation[3] = 0;
	creature->Alerted = false;
	creature->HeadLeft = false;
	creature->HeadRight = false;
	creature->ReachedGoal = false;
	creature->HurtByLara = false;
	creature->Patrol = false;
	creature->JumpAhead = false;
	creature->MonkeySwingAhead = false;
	creature->MaxTurn = ANGLE(1);
	creature->Flags = 0;
	creature->Enemy = nullptr;

	// Initialize LOT defaults.
	creature->LOT.CanJump = false;
	creature->LOT.CanMonkey = false;
	creature->LOT.IsJumping = false;
	creature->LOT.IsMonkeying = false;
	creature->LOT.Fly = NO_FLYING;
	creature->LOT.BlockMask = BLOCKED;
	creature->AITargetNumber = NO_VALUE;
	creature->AITarget = nullptr;

	// Create AI target item if needed (for AI_FOLLOW, AI_PATROL, etc.)
	if (makeTarget)
	{
		creature->AITargetNumber = CreateItem();
		if (creature->AITargetNumber != NO_VALUE)
			creature->AITarget = &g_Level.Items[creature->AITargetNumber];
	}

	// Set movement capabilities based on object's LotType.
	// Each LotType defines what terrain the creature can traverse.
	switch (object->LotType)
	{
		default:
		case LotType::Basic:
			// Basic ground creatures: small step up, medium drop down.
			// Examples: dogs, rats, small animals.
			creature->LOT.Step = CLICK(1);   // Can step up 1 click (256 units).
			creature->LOT.Drop = -CLICK(2);  // Can drop down 2 clicks (512 units).
			creature->LOT.Zone = ZoneType::Basic;
			break;

		case LotType::Skeleton:
			// Skeleton-type creatures that can jump gaps.
			// Uses separate Skeleton zone for pathfinding.
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -CLICK(2);
			creature->LOT.CanJump = true;    // Can use BOX_JUMP overlaps.
			creature->LOT.Zone = ZoneType::Skeleton;
			break;

		case LotType::Flyer:
			// Flying creatures (bats, harpies, etc.)
			// Huge step/drop allows reaching any height.
			// Fly value controls vertical movement speed.
			creature->LOT.Step = BLOCK(20);  // Can reach 20 blocks up.
			creature->LOT.Drop = -BLOCK(20); // Can reach 20 blocks down.
			creature->LOT.Fly = DEFAULT_FLY_UPDOWN_SPEED;
			creature->LOT.Zone = ZoneType::Flyer;
			break;

		case LotType::Water:
			// Underwater creatures (sharks, fish, etc.)
			// Can only exist in water, huge vertical range.
			creature->LOT.Step = BLOCK(20);
			creature->LOT.Drop = -BLOCK(20);
			creature->LOT.Zone = ZoneType::Water;
			creature->LOT.Fly = DEFAULT_SWIM_UPDOWN_SPEED; // Swim speed.
			break;

		case LotType::Amphibious:
			// Creatures that can swim AND walk on land (crocodile, big rat).
			// Uses Amphibious zone which includes both water and land boxes.
			creature->LOT.Step = BLOCK(20);
			creature->LOT.Drop = -BLOCK(20);
			creature->LOT.Zone = ZoneType::Amphibious;

			// Different swim speeds per creature type.
			if (item->ObjectNumber == ID_CROCODILE)
			{
				creature->LOT.Fly = DEFAULT_SWIM_UPDOWN_SPEED / 2; // Slower swimmer.
			}
			else if (item->ObjectNumber == ID_BIG_RAT)
			{
				creature->LOT.Fly = NO_FLYING; // Surface swimmer only, can't dive.
			}
			else
			{
				creature->LOT.Fly = DEFAULT_SWIM_UPDOWN_SPEED;
			}
			break;

		case LotType::SnowmobileGun:
			// Vehicle-mounted gunner, limited mobility.
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.Zone = ZoneType::Human;
			break;

		case LotType::Human:
			// Human enemies that can climb 1 block ledges.
			// Examples: guards, soldiers, workers.
			creature->LOT.Step = BLOCK(1);   // Can climb 1 block (1024 units).
			creature->LOT.Drop = -BLOCK(1);  // Can drop 1 block.
			creature->LOT.Zone = ZoneType::Human;
			break;

		case LotType::HumanPlusJump:
			// Humans that can also jump gaps.
			// Examples: agile enemies, ninjas.
			creature->LOT.Step = BLOCK(1);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.CanJump = true;
			creature->LOT.Zone = ZoneType::Human;
			break;

		case LotType::HumanPlusJumpAndMonkey:
			// Most agile human type: climb, jump, and monkeyswing.
			// Examples: special forces, acrobatic enemies.
			creature->LOT.Step = BLOCK(1);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.CanJump = true;
			creature->LOT.CanMonkey = true;  // Can use BOX_MONKEY overlaps.
			creature->LOT.Zone = ZoneType::Human;
			break;

		case LotType::Spider:
			// Spiders can climb slightly higher steps.
			creature->LOT.Step = CLICK(2);   // 2 clicks step.
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.Zone = ZoneType::Human;
			break;

		case LotType::Blockable:
			// Creatures that can be blocked by BLOCKABLE flag on boxes.
			// Used for creatures that should respect certain barriers.
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -CLICK(2);
			creature->LOT.BlockMask = BLOCKABLE; // Respects BLOCKABLE boxes.
			creature->LOT.Zone = ZoneType::Basic;
			break;

		case LotType::Ape:
			// Apes have slightly better climbing than basic.
			creature->LOT.Step = CLICK(2);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.Zone = ZoneType::Human;
			break;
	}

	// Reset pathfinding state for fresh start.
	ClearLOT(&creature->LOT);

	// Create zone data (skip for Lara - she doesn't use creature AI).
	if (itemNumber != LaraItem->Index)
		CreateZone(item);
}

/**
 * @brief Finds and sets the nearest valid entity as the creature's enemy.
 *
 * Searches through all active creatures and the player to find the closest
 * valid target. Used for creature-vs-creature combat and targeting systems.
 *
 * @param item The creature looking for a target.
 * @param keyObjectIds List of object IDs to specifically target or ignore.
 * @param ignoreKeyObjectIds If true, ignores objects in keyObjectIds; if false, ONLY targets them.
 */
void TargetNearestEntity(ItemInfo& item, const std::vector<GAME_OBJECT_ID>& keyObjectIds, bool ignoreKeyObjectIds)
{
	auto& creature = *GetCreatureInfo(&item);
	creature.Enemy = nullptr;

	float closestDistSqr = FLT_MAX;
	for (auto creatureIndex : ActiveCreatures)
	{
		// Don't target itself.
		if (creatureIndex == item.Index)
			continue;

		auto& targetItem = g_Level.Items[creatureIndex];

		// Don't target same object type.
		if (item.ObjectNumber == targetItem.ObjectNumber)
			continue;

		// Ignore or specifically target key object IDs.
		if (!keyObjectIds.empty() && (ignoreKeyObjectIds ? Contains(keyObjectIds, targetItem.ObjectNumber) : !Contains(keyObjectIds, targetItem.ObjectNumber)))
			continue;

		// Ignore undead enemies (can't be damaged normally).
		if (Objects[targetItem.ObjectNumber].damageType != DamageMode::Any)
			continue;

		if (&targetItem != &item && targetItem.HitPoints > 0 && targetItem.Status != ITEM_INVISIBLE)
		{
			float distSqr = Vector3i::DistanceSquared(item.Pose.Position, targetItem.Pose.Position);
			if (distSqr < closestDistSqr)
			{
				creature.Enemy = &targetItem;
				closestDistSqr = distSqr;
			}
		}
	}

	// Handle player as special case (not in ActiveCreatures list).
	if (!keyObjectIds.empty() && (ignoreKeyObjectIds ? Contains(keyObjectIds, ID_LARA) : !Contains(keyObjectIds, ID_LARA)))
		return;

	float distToPlayerSqr = Vector3i::DistanceSquared(item.Pose.Position, LaraItem->Pose.Position);
	if (distToPlayerSqr < closestDistSqr)
		creature.Enemy = LaraItem;
}

/**
 * @brief Sets a specific item as the creature's AI target.
 *
 * Used to direct a creature to pathfind toward a specific item rather than
 * its enemy. Commonly used with AI_FOLLOW, AI_PATROL nullpoints.
 *
 * @param itemNum Index of the creature.
 * @param target Index of the target item, or NO_VALUE to clear.
 */
void SetEntityTarget(short itemNum, short target)
{
	auto* item = &g_Level.Items[itemNum];
	auto* creature = GetCreatureInfo(item);

	creature->AITargetNumber = target;

	if (creature->AITargetNumber != NO_VALUE)
		creature->AITarget = &g_Level.Items[creature->AITargetNumber];
	else
		creature->AITarget = nullptr;
}

/**
 * @brief Resets the LOT pathfinding state for a new search.
 *
 * Clears the search queue (Head/Tail) and resets all node data. Must be called
 * before starting a new pathfinding search or when the creature's zone changes.
 *
 * The search uses a linked-list queue structure:
 * - Head: First box to expand in the search.
 * - Tail: Last box in the expansion queue.
 * - Each node's nextExpansion links to the next box to process.
 *
 * @param LOT Pointer to the LOTInfo structure to clear.
 */
void ClearLOT(LOTInfo* LOT)
{
	// Clear the BFS queue pointers.
	LOT->Head = NO_VALUE;  // First box to expand (front of queue).
	LOT->Tail = NO_VALUE;  // Last box in queue (for appending).

	// Reset search state.
	LOT->SearchNumber = 0;   // Incremented each new search to invalidate old data.
	LOT->TargetBox = NO_VALUE;    // The box we're pathfinding TO.
	LOT->RequiredBox = NO_VALUE;  // The box we WANT to pathfind to (triggers recalc).

	// Reset all node data.
	auto* node = LOT->Node.data();
	for (auto& node : LOT->Node)
	{
		node.exitBox = NO_VALUE;       // Direction to target (filled by SearchLOT).
		node.nextExpansion = NO_VALUE; // Next node in BFS queue.
		node.searchNumber = 0;         // Which search this node was visited in.
	}
}

/**
 * @brief Creates the navigable zone for a creature based on its starting position.
 *
 * This function determines which boxes the creature can potentially reach by checking
 * zone connectivity. Zones are pre-computed arrays that group boxes together based on
 * reachability for different creature types.
 *
 * Zone System Explanation:
 * - Each box has a zone number for each ZoneType (Basic, Skeleton, Water, Human, Flyer, Amphibious).
 * - Boxes with the SAME zone number are considered connected/reachable for that creature type.
 * - There are two zone arrays per type: normal and flipped (for flipmap support).
 *
 * The function populates LOT.Node[] with box numbers that share the same zone as the
 * creature's starting box. This creates a subset of boxes the creature can pathfind to.
 *
 * Special Cases:
 * - Flyers: Can reach ANY box (bypass zone filtering entirely).
 * - Water/Amphibious: Use their respective zone arrays for connectivity.
 *
 * @param item Pointer to the creature item.
 */
void CreateZone(ItemInfo* item)
{
	auto* creature = GetCreatureInfo(item);
	auto* room = &g_Level.Rooms[item->RoomNumber];
	auto& object = Objects[item->ObjectNumber];

	// Determine which box the creature is currently standing on.
	// This uses the sector grid within the room to find the pathfinding box ID.
	item->BoxNumber = GetSector(room, item->Pose.Position.x - room->Position.x, item->Pose.Position.z - room->Position.z)->PathfindingBoxID;

	// SPECIAL CASE: Flying creatures bypass zone filtering entirely.
	// They can fly to any box in the level regardless of connectivity.
	if (object.LotType == LotType::Flyer)
	{
		auto* node = creature->LOT.Node.data();
		creature->LOT.ZoneCount = 0;

		// Add ALL boxes to the creature's navigable set.
		for (int i = 0; i < g_Level.PathfindingBoxes.size(); i++)
		{
			node->boxNumber = i;
			node++;
			creature->LOT.ZoneCount++;
		}
	}
	else
	{
		// NORMAL CASE: Use zone filtering to determine reachable boxes.
		// Zones are pre-computed arrays that group connected boxes together.

		// Get zone arrays for this creature type.
		// [0] = normal zones, [1] = flipped zones (for flipmap support).
		int* zone = g_Level.Zones[(int)creature->LOT.Zone][0].data();
		int* flippedZone = g_Level.Zones[(int)creature->LOT.Zone][1].data();

		// Get the zone number of the creature's starting box.
		// Only boxes with matching zone numbers are considered reachable.
		int zoneNumber = zone[item->BoxNumber];
		int flippedZoneNumber = flippedZone[item->BoxNumber];

		auto* node = creature->LOT.Node.data();
		creature->LOT.ZoneCount = 0;

		// Iterate through ALL boxes and add those in the same zone.
		// This builds the creature's "reachable set" of boxes.
		for (int i = 0; i < g_Level.PathfindingBoxes.size(); i++)
		{
			// Check both normal and flipped zone (supports flipmaps).
			// A box is reachable if it matches EITHER zone state.
			if (*zone == zoneNumber || *flippedZone == flippedZoneNumber)
			{
				node->boxNumber = i;
				node++;
				creature->LOT.ZoneCount++;
			}

			zone++;
			flippedZone++;
		}
	}
}
