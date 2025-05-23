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

#define DEFAULT_FLY_UPDOWN_SPEED 16
#define DEFAULT_SWIM_UPDOWN_SPEED 32

int SlotsUsed;
std::vector<CreatureInfo*> ActiveCreatures;

void InitializeLOTarray(int itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];
	auto* creature = GetCreatureInfo(item);

	if (!creature->LOT.Initialized)
	{
		creature->LOT.Node = std::vector<BoxNode>(g_Level.PathfindingBoxes.size(), BoxNode{});
		creature->LOT.Initialized = true;
	}
}

bool EnableEntityAI(short itemNum, bool always, bool makeTarget)
{
	ItemInfo* item = &g_Level.Items[itemNum];

	if (item->IsCreature())
		return true;

	InitializeSlot(itemNum, makeTarget);
	ActiveCreatures.push_back(item->Data);

	return item->IsCreature();
}

void DisableEntityAI(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (!item->IsCreature())
		return;

	auto* creature = GetCreatureInfo(item);
	creature->ItemNumber = NO_VALUE;
	KillItem(creature->AITargetNumber);
	ActiveCreatures.erase(std::find(ActiveCreatures.begin(), ActiveCreatures.end(), creature));
	item->Data = nullptr;
}

void InitializeSlot(short itemNumber, bool makeTarget)
{
	auto* item = &g_Level.Items[itemNumber];
	auto* object = &Objects[item->ObjectNumber];
	item->Data = CreatureInfo();
	auto* creature = GetCreatureInfo(item);

	InitializeLOTarray(itemNumber);
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

	creature->LOT.CanJump = false;
	creature->LOT.CanMonkey = false;
	creature->LOT.IsAmphibious = false; // True for crocodile by default as only the crocodile can move in water and on land.
	creature->LOT.IsJumping = false;
	creature->LOT.IsMonkeying = false;
	creature->LOT.Fly = NO_FLYING;
	creature->LOT.BlockMask = BLOCKED;
	creature->AITargetNumber = NO_VALUE;
	creature->AITarget = nullptr;

	if (makeTarget)
	{
		creature->AITargetNumber = CreateItem();
		if (creature->AITargetNumber != NO_VALUE)
			creature->AITarget = &g_Level.Items[creature->AITargetNumber];
	}

	switch (object->LotType)
	{
		default:
		case LotType::Basic:
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -CLICK(2);
			creature->LOT.Zone = ZoneType::Basic;
			break;

		// Can jump.
		case LotType::Skeleton:
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -CLICK(2);
			creature->LOT.CanJump = true;
			creature->LOT.Zone = ZoneType::Skeleton;
			break;

		// Can fly.
		case LotType::Flyer:
			creature->LOT.Step = BLOCK(20);
			creature->LOT.Drop = -BLOCK(20);
			creature->LOT.Fly = DEFAULT_FLY_UPDOWN_SPEED;
			creature->LOT.Zone = ZoneType::Flyer;
			break;

		// Can swim.
		case LotType::Water:
			creature->LOT.Step = BLOCK(20);
			creature->LOT.Drop = -BLOCK(20);
			creature->LOT.Zone = ZoneType::Water;

			if (item->ObjectNumber == ID_CROCODILE)
			{
				creature->LOT.Fly = DEFAULT_SWIM_UPDOWN_SPEED / 2; // Slower than the other underwater creatures.
				creature->LOT.IsAmphibious = true;				   // Can walk and swim.
			}
			else if (item->ObjectNumber == ID_BIG_RAT)
			{
				creature->LOT.Fly = NO_FLYING;	   // Can't swim underwater, only on the surface.
				creature->LOT.IsAmphibious = true; // Can walk and swim.
			}
			else
			{
				creature->LOT.Fly = DEFAULT_SWIM_UPDOWN_SPEED;
			}

			break;

		case LotType::SnowmobileGun:
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.Zone = ZoneType::Human;
			break;

		// Can climb.
		case LotType::Human:
			creature->LOT.Step = BLOCK(1);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.Zone = ZoneType::Human;
			break;

		// Can climb and jump.
		case LotType::HumanPlusJump:
			creature->LOT.Step = BLOCK(1);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.CanJump = true;
			creature->LOT.Zone = ZoneType::Human;
			break;

		// Can climb, jump, monkeyswing.
		case LotType::HumanPlusJumpAndMonkey:
			creature->LOT.Step = BLOCK(1);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.CanJump = true;
			creature->LOT.CanMonkey = true;
			creature->LOT.Zone = ZoneType::Human;
			break;

		case LotType::Spider:
			creature->LOT.Step = CLICK(2);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.Zone = ZoneType::Human;
			break;

		case LotType::Blockable:
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -CLICK(2);
			creature->LOT.BlockMask = BLOCKABLE;
			creature->LOT.Zone = ZoneType::Basic;
			break;

		case LotType::Ape:
			creature->LOT.Step = CLICK(2);
			creature->LOT.Drop = -BLOCK(1);
			creature->LOT.Zone = ZoneType::Human;
			break;
	}

	ClearLOT(&creature->LOT);
	if (itemNumber != LaraItem->Index)
		CreateZone(item);

	SlotsUsed++;
}

void TargetNearestEntity(ItemInfo& item, const std::vector<GAME_OBJECT_ID>& keyObjectIds, bool ignoreKeyObjectIds)
{
	auto& creature = *GetCreatureInfo(&item);

	float closestDistSqr = INFINITY;
	for (auto& target : ActiveCreatures)
	{
		auto& targetItem = g_Level.Items[target->ItemNumber];
		if (targetItem.Index == item.Index)
			continue;

		// Ignore or specifically target key object IDs.
		if (!keyObjectIds.empty() && (ignoreKeyObjectIds ? Contains(keyObjectIds, targetItem.ObjectNumber) : !Contains(keyObjectIds, targetItem.ObjectNumber)))
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

	// Handle player as special case.
	if (!keyObjectIds.empty() && (ignoreKeyObjectIds ? Contains(keyObjectIds, ID_LARA) : !Contains(keyObjectIds, ID_LARA)))
		return;

	float distToPlayerSqr = Vector3i::DistanceSquared(item.Pose.Position, LaraItem->Pose.Position);
	if (distToPlayerSqr < closestDistSqr)
		creature.Enemy = LaraItem;
}

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

void ClearLOT(LOTInfo* LOT)
{
	LOT->Head = NO_VALUE;
	LOT->Tail = NO_VALUE;
	LOT->SearchNumber = 0;
	LOT->TargetBox = NO_VALUE;
	LOT->RequiredBox = NO_VALUE;

	auto* node = LOT->Node.data();
	for (auto& node : LOT->Node) 
	{
		node.exitBox = NO_VALUE;
		node.nextExpansion = NO_VALUE;
		node.searchNumber = 0;
	}
}

void CreateZone(ItemInfo* item)
{
	auto* creature = GetCreatureInfo(item);
	auto* room = &g_Level.Rooms[item->RoomNumber];

	item->BoxNumber = GetSector(room, item->Pose.Position.x - room->Position.x, item->Pose.Position.z - room->Position.z)->PathfindingBoxID;

	if (creature->LOT.Fly)
	{
		auto* node = creature->LOT.Node.data();
		creature->LOT.ZoneCount = 0;

		for (int i = 0; i < g_Level.PathfindingBoxes.size(); i++)
		{
			node->boxNumber = i;
			node++;
			creature->LOT.ZoneCount++;
		}
	}
	else
	{
		int* zone = g_Level.Zones[(int)creature->LOT.Zone][0].data();
		int* flippedZone = g_Level.Zones[(int)creature->LOT.Zone][1].data();

		int zoneNumber = zone[item->BoxNumber];
		int flippedZoneNumber = flippedZone[item->BoxNumber];

		auto* node = creature->LOT.Node.data();
		creature->LOT.ZoneCount = 0;

		for (int i = 0; i < g_Level.PathfindingBoxes.size(); i++)
		{
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
