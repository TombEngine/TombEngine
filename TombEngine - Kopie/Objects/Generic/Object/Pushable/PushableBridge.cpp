#include "framework.h"
#include "Objects/Generic/Object/Pushable/PushableBridge.h"

#include "Game/collision/floordata.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;

namespace TEN::Entities::Generic
{
	std::optional<int> GetPushableBridgeFloorHeight(const ItemInfo& item, const Vector3i& pos)
	{
		const auto& pushable = GetPushableInfo(item);

		auto boxHeight = GetBridgeItemIntersect(item, pos, false);

		if (item.Active &&item.Status != ITEM_INVISIBLE &&
			pushable.UseRoomCollision &&
			boxHeight.has_value())
		{
			int height = item.Pose.Position.y - GetPushableHeight(item);
			return height;
		}

		return std::nullopt;
	}

	std::optional<int> GetPushableBridgeCeilingHeight(const ItemInfo& item, const Vector3i& pos)
	{
		const auto& pushable = GetPushableInfo(item);

		auto boxHeight = GetBridgeItemIntersect(item, pos, true);

		if (item.Active && item.Status != ITEM_INVISIBLE &&
			pushable.UseRoomCollision &&
			boxHeight.has_value())
		{
			return item.Pose.Position.y;
		}

		return std::nullopt;
	}

	int GetPushableBridgeFloorBorder(const ItemInfo& item)
	{
		return (item.Pose.Position.y - GetPushableHeight(item));
	}

	int GetPushableBridgeCeilingBorder(const ItemInfo& item)
	{
		return item.Pose.Position.y;
	}

	void EnablePushableBridge(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		if (pushable.UseRoomCollision && pushable.Bridge.has_value())
		{
			pushable.UseBridgeCollision = true;

			auto& bridge = GetBridgeObject(pushableItem);
			bridge.Enable(pushableItem);
		}
	}

	void EnablePushableStackBridge(ItemInfo& pushableItem, bool addBridge)
	{
		auto* currentPushableItem = &g_Level.Items[pushableItem.Index];
		auto* currentPushable = &GetPushableInfo(*currentPushableItem);

		// NOTE: Can't stack pushable object on pushable bridge.
		if (!currentPushable->UseRoomCollision)
			return;

		if (currentPushable->UseBridgeCollision && currentPushable->Bridge.has_value())
			addBridge ? currentPushable->Bridge->Enable(pushableItem) : currentPushable->Bridge->Disable(pushableItem);
		
		while (currentPushable->Stack.ItemNumberAbove != NO_VALUE)
		{
			currentPushableItem = &g_Level.Items[currentPushable->Stack.ItemNumberAbove];
			currentPushable = &GetPushableInfo(*currentPushableItem);

			if (currentPushable->UseBridgeCollision && currentPushable->Bridge.has_value())
				addBridge ? currentPushable->Bridge->Enable(*currentPushableItem) : currentPushable->Bridge->Disable(*currentPushableItem);
		}
	}

	void DisablePushableBridge(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		if (pushable.UseRoomCollision && pushable.Bridge.has_value())
		{
			pushable.UseBridgeCollision = false;

			auto& bridge = GetBridgeObject(pushableItem);
			bridge.Disable(pushableItem);
		}
	}
}
