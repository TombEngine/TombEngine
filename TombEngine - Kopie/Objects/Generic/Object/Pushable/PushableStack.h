#pragma once

struct ItemInfo;

namespace TEN::Entities::Generic
{
	void InitializePushableStacks();
	std::vector<int> FindAllPushables(const std::vector<ItemInfo>& items);

	void StackPushable(int itemNumber, int targetItemNumber);
	void UnstackPushable(int itemNumber);
	
	int SearchNearPushablesStack(int itemNumber);
	int FindPushableStackInRoom(int itemNumber, int roomNumber);

	int GetPushableCountInStack(int itemNumber);
	bool IsWithinStackLimit(int itemNumber);

	int GetStackHeight(int itemNumber);

	void StartMovePushableStack(int itemNumber);
	void StopMovePushableStack(int itemNumber);

	void StartFallPushableStack(int itemNumber);
	void StopFallPushableStack(int itemNumber);

	void SetPushableVerticalPos(const ItemInfo& pushableItem, int deltaY);
}