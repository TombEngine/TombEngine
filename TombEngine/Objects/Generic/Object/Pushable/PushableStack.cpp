#include "framework.h"
#include "Objects/Generic/Object/Pushable/PushableStack.h"

#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/Pushable/PushableBridge.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;

namespace TEN::Entities::Generic
{
	// NOTE: Required to use Vector3i in std::unordered_map.
	struct Vector3iHasher 
	{
		std::size_t operator()(const Vector3i& vector) const 
		{
			std::size_t h1 = std::hash<int>()(vector.x);
			std::size_t h2 = std::hash<int>()(vector.y);
			std::size_t h3 = std::hash<int>()(vector.z);
			return (h1 ^ (h2 << 1) ^ (h3 << 2));
		}
	};

	void InitializePushableStacks()
	{
		// 1) Collect all pushables in level.
		auto& pushableItemNumbers = FindAllPushables(g_Level.Items);
		if (pushableItemNumbers.empty())
			return;

		// 2) Prepare data to create several lists per each stack group (one for each XZ and ground floor height).
		// PROBLEM: Missing hash function in Vector3i, creating custom version Vector3iHasher at the top of this source code.
		auto stackGroups = std::unordered_map<Vector3i, std::vector<int>, Vector3iHasher>{}; // stack Position - pushable itemNumber  

		// 3) Iterate through the pushables list, to put them in their different stack groups. According to their XZ and ground floor height).
		// Extra, I moved also the .Data initialization here, to can store data in all the pushables objects (even the ones not initialized yet).
		for (int itemNumber : pushableItemNumbers)
		{
			auto& pushableItem = g_Level.Items[itemNumber];
			pushableItem.Data = PushableInfo();

			// Initialize bridge routines.
			auto& pushable = GetPushableInfo(pushableItem);
			if (pushable.Bridge.has_value())
			{
				pushable.Bridge->GetFloorHeight = GetPushableBridgeFloorHeight;
				pushable.Bridge->GetCeilingHeight = GetPushableBridgeCeilingHeight;
				pushable.Bridge->GetFloorBorder = GetPushableBridgeFloorBorder;
				pushable.Bridge->GetCeilingBorder = GetPushableBridgeCeilingBorder;
			}
			
			int x = pushableItem.Pose.Position.x;
			int z = pushableItem.Pose.Position.z;
			
			auto pointColl = GetPointCollision(pushableItem);
			int y = pointColl.GetFloorHeight();
			
			stackGroups.emplace(Vector3i(x, y, z), std::vector<int>()).first->second.push_back(itemNumber);
		}

		// 4) Iterate through stack groups lists, sort each by vertical position, and iterate to make stack links.
		for (auto& group : stackGroups)
		{
			auto& pushablesInGroup = group.second;

			// If only 1 pushable in that position, no stack check required.
			if (pushablesInGroup.size() <= 1)
				continue;

			// If 2 or more pushables in group, sort from bottom to top (highest Y to Lowest Y).
			std::sort(pushablesInGroup.begin(), pushablesInGroup.end(), [](int pushableItemNumber0, int pushableItemNumber1) 
				{
					const auto& pushable0 = g_Level.Items[pushableItemNumber0];
					const auto& pushable1 = g_Level.Items[pushableItemNumber1];

					return (pushable0.Pose.Position.y > pushable1.Pose.Position.y);
				});

			// Iterate through each group to set the stack links).
			for (int i = 0; i < (pushablesInGroup.size() - 1); ++i)
			{
				// Set stackUpperItem and stackLowerItem variables accordingly.
				int lowerItemNumber = pushablesInGroup[i];
				auto& lowerPushableItem = g_Level.Items[lowerItemNumber];
				auto& lowerPushable = GetPushableInfo(lowerPushableItem);
				
				int upperItemNumber = pushablesInGroup[i + 1];
				auto& upperPushableItem = g_Level.Items[upperItemNumber];
				auto& upperPushable = GetPushableInfo(upperPushableItem);
				
				lowerPushable.Stack.ItemNumberAbove = upperItemNumber;
				upperPushable.Stack.ItemNumberBelow = lowerItemNumber;
			}
		}
	}

	std::vector<int> FindAllPushables(const std::vector<ItemInfo>& items)
	{
		auto pushableItemNumbers = std::vector<int>{};
		for (int i = 0; i < items.size(); i++)
		{
			auto& item = items[i];

			if ((item.ObjectNumber >= ID_PUSHABLE_OBJECT1 && item.ObjectNumber <= ID_PUSHABLE_OBJECT10) ||
				(item.ObjectNumber >= ID_PUSHABLE_OBJECT_CLIMBABLE_1 && item.ObjectNumber <= ID_PUSHABLE_OBJECT_CLIMBABLE_10))
			{
				pushableItemNumbers.push_back(i);
			}
		}

		return pushableItemNumbers;
	}

	void StackPushable(int itemNumber, int targetItemNumber)
	{
		if (targetItemNumber == NO_VALUE)
			return;

		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		pushable.Stack.ItemNumberBelow = targetItemNumber;

		auto& lowerPushableItem = g_Level.Items[targetItemNumber];
		auto& lowerPushable = GetPushableInfo(lowerPushableItem);

		lowerPushable.Stack.ItemNumberAbove = itemNumber;
	}

	void UnstackPushable(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		if (pushable.Stack.ItemNumberBelow == NO_VALUE)
			return;

		auto& lowerPushableItem = g_Level.Items[pushable.Stack.ItemNumberBelow];
		auto& lowerPushable = GetPushableInfo(lowerPushableItem);

		pushable.Stack.ItemNumberBelow = NO_VALUE;
		lowerPushable.Stack.ItemNumberAbove = NO_VALUE;
	}

	int SearchNearPushablesStack(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];

		int pushabelStackFound = FindPushableStackInRoom(itemNumber, pushableItem.RoomNumber);

		if (pushabelStackFound != NO_VALUE)
			return pushabelStackFound;

		// Otherwise, check room below.
		//auto collisionResult = GetPointCollision(pushableItem.Pose.Position.x, pushableItem.Pose.Position.y, pushableItem.Pose.Position.z, pushableItem.RoomNumber);		
		//auto roomNumberBelow = collisionResult.Block->GetRoomNumberBelow(pushableItem.Pose.Position.x, pushableItem.Pose.Position.y, pushableItem.Pose.Position.z).value();
		//pushabelStackFound = FindPushableStackInRoom(itemNumber, roomNumberBelow);

		return NO_VALUE;
	}

	int FindPushableStackInRoom(int itemNumber, int roomNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		if (roomNumber != NO_VALUE)
		{
			short currentItemNumber = g_Level.Rooms[roomNumber].itemNumber;
			while (currentItemNumber != NO_VALUE)
			{
				auto& currentItem = g_Level.Items[currentItemNumber];

				// If climbable, is in same XZ position, and at lower height.
				if ((currentItem.ObjectNumber >= ID_PUSHABLE_OBJECT_CLIMBABLE_1 && currentItem.ObjectNumber <= ID_PUSHABLE_OBJECT_CLIMBABLE_10) &&
					(currentItem.Pose.Position.x == pushableItem.Pose.Position.x) && (currentItem.Pose.Position.z == pushableItem.Pose.Position.z) &&
					(currentItem.Pose.Position.y > pushableItem.Pose.Position.y))
				{
					// Find top item.
					if (pushable.Stack.ItemNumberAbove == NO_VALUE)
					{
						return currentItemNumber;
					}
					else
					{
						int topItemNumber = pushable.Stack.ItemNumberAbove;
						while (topItemNumber != NO_VALUE)
						{
							auto& topItem = g_Level.Items[topItemNumber];
							auto& topPushable = GetPushableInfo(topItem);

							if (topPushable.Stack.ItemNumberAbove == NO_VALUE)
							{
								return topItemNumber;
							}
							else
							{
								topItemNumber = topPushable.Stack.ItemNumberAbove;
							}
						}
					}
				}
				else
				{
					currentItemNumber = currentItem.NextItem;
				}
			}
		}

		return NO_VALUE;
	}

	int GetPushableCountInStack(int itemNumber)
	{
		auto pushableItemCopy = g_Level.Items[itemNumber];
		auto& pushableCopy = GetPushableInfo(pushableItemCopy);

		int count = 1;
		while (pushableCopy.Stack.ItemNumberAbove != NO_VALUE)
		{
			// Filter out current pushable item.
			if (pushableCopy.Stack.ItemNumberAbove == itemNumber)
				break;

			pushableItemCopy = g_Level.Items[pushableCopy.Stack.ItemNumberAbove];
			pushableCopy = GetPushableInfo(pushableItemCopy);

			count++;
		}

		return count;
	}

	bool IsWithinStackLimit(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		auto count = GetPushableCountInStack(itemNumber);
		return (count <= pushable.Stack.Limit);
	}

	int GetStackHeight(int itemNumber)
	{
		auto pushableItemCopy = g_Level.Items[itemNumber];
		auto& pushableCopy = GetPushableInfo(pushableItemCopy);

		int totalHeight = pushableCopy.Height;

		while (pushableCopy.Stack.ItemNumberAbove != NO_VALUE)
		{
			pushableItemCopy = g_Level.Items[pushableCopy.Stack.ItemNumberAbove];
			pushableCopy = GetPushableInfo(pushableItemCopy);

			totalHeight = totalHeight + pushableCopy.Height;
		}

		return totalHeight;
	}

	void StartMovePushableStack(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		int currentItemNumber = pushable.Stack.ItemNumberAbove;
		while (currentItemNumber != NO_VALUE)
		{
			auto& currentPushableItem = g_Level.Items[currentItemNumber];
			auto& currentPushable = GetPushableInfo(currentPushableItem);

			// Disable bridge.
			if (currentPushable.UseRoomCollision && currentPushable.Bridge.has_value())
				currentPushable.Bridge->Disable(currentPushableItem);

			currentPushable.BehaviorState = PushableBehaviorState::MoveStackHorizontal;

			currentItemNumber = currentPushable.Stack.ItemNumberAbove;
		}
	}

	void StopMovePushableStack(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		int currentItemNumber = pushable.Stack.ItemNumberAbove;
		while (currentItemNumber != NO_VALUE)
		{
			auto& currentPushableItem = g_Level.Items[currentItemNumber];
			auto& currentPushable = GetPushableInfo(currentPushableItem);

			currentPushableItem.Pose.Position = GetNearestSectorCenter(currentPushableItem.Pose.Position);

			// Enable bridge.
			if (currentPushable.UseRoomCollision && currentPushable.Bridge.has_value())
				currentPushable.Bridge->Enable(currentPushableItem);

			currentPushable.BehaviorState = PushableBehaviorState::Idle;

			currentItemNumber = currentPushable.Stack.ItemNumberAbove;
		}
	}
	
	void StartFallPushableStack(int itemNumber)
	{

	}

	void StopFallPushableStack(int itemNumber)
	{

	}

	// TODO: Problems with bridge collision.
	void SetPushableVerticalPos(const ItemInfo& pushableItem, int relHeight)
	{
		auto* pushableItemPtr = &g_Level.Items[pushableItem.Index];
		const auto* pushablePtr = &GetPushableInfo(*pushableItemPtr);

		while (pushablePtr->Stack.ItemNumberAbove != NO_VALUE)
		{
			if (pushablePtr->Stack.ItemNumberAbove == pushableItem.Index)
				break;

			pushableItemPtr->Pose.Position.y += relHeight;

			pushableItemPtr = &g_Level.Items[pushablePtr->Stack.ItemNumberAbove];
			pushablePtr = &GetPushableInfo(*pushableItemPtr);
		}
	}
}
