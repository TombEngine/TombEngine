#include "framework.h"
#include "Objects/TR5/Object/tr5_raisingblock.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Objects/objectslist.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Math;

namespace TEN::Entities::Generic
{
	static std::optional<int> GetRaisingBlockFloorHeight(const ItemInfo& item, const Vector3i& pos)
	{
		auto bboxHeight = GetBridgeItemIntersect(item, pos, false);
		if (bboxHeight.has_value())
		{
			auto bounds = GameBoundingBox(&item);
			int height = bounds.GetHeight();

			int currentHeight = item.Pose.Position.y - ((height * item.ItemFlags[1]) / BLOCK(4));
			return currentHeight;
		}

		return bboxHeight;
	}

	static std::optional<int> GetRaisingBlockCeilingHeight(const ItemInfo& item, const Vector3i& pos)
	{
		auto bboxHeight = GetBridgeItemIntersect(item, pos, true);

		if (bboxHeight.has_value())
		{
			return (bboxHeight.value() + 1);
		}
		else
		{
			return bboxHeight;
		}
	}

	static int GetRaisingBlockFloorBorder(const ItemInfo& item)
	{
		return GetBridgeBorder(item, false);
	}

	static int GetRaisingBlockCeilingBorder(const ItemInfo& item)
	{
		return GetBridgeBorder(item, true);
	}

	void InitializeRaisingBlock(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		item->Data = BridgeObject();
		auto& bridge = GetBridgeObject(*item);

		short roomNumber = item->RoomNumber;
		auto* floor = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &roomNumber);

		if (floor->PathfindingBoxID != NO_VALUE)
			g_Level.PathfindingBoxes[floor->PathfindingBoxID].flags &= ~BLOCKED;

		// Set Y scale to 0 by default.
		item->Pose.Scale.y = 0.0f;

		if (item->TriggerFlags < 0)
		{
			item->AIBits |= ALL_AIOBJ;
			AddActiveItem(itemNumber);
			item->Status = ITEM_ACTIVE;
		}

		bridge.GetFloorHeight = GetRaisingBlockFloorHeight;
		bridge.GetCeilingHeight = GetRaisingBlockCeilingHeight;
		bridge.GetFloorBorder = GetRaisingBlockFloorBorder;
		bridge.GetCeilingBorder = GetRaisingBlockCeilingBorder;
		bridge.Initialize(*item);
	}

	void ShakeRaisingBlock(ItemInfo* item)
	{
		SoundEffect(SFX_TR4_RAISING_BLOCK_2, &item->Pose);

		if (item->TriggerFlags == 0)
			return;

		if ((item->Pose.Position.ToVector3() - Camera.pos.ToVector3()).Length() < BLOCK(10))
		{
			if (item->ItemFlags[1] == 64 || item->ItemFlags[1] == 4096)
			{
				Camera.bounce = -32;
			}
			else
			{
				Camera.bounce = -16;
			}
		}
	}

	void ControlRaisingBlock(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		auto& bridge = GetBridgeObject(*item);

		if (TriggerActive(item))
		{
			if (!item->ItemFlags[2])
				item->ItemFlags[2] = 1;

			if (item->TriggerFlags < 0)
			{
				item->ItemFlags[1] = 1;
			}
			else if (item->ItemFlags[1] < 4096)
			{
				ShakeRaisingBlock(item);
				item->ItemFlags[1] += 64;
			}
		}
		else if (item->ItemFlags[1] <= 0 || item->TriggerFlags < 0)
		{
			if (item->ItemFlags[2])
			{
				item->ItemFlags[1] = 0;

				if (item->ObjectNumber == ID_RAISING_BLOCK1)
				{
					if (item->TriggerFlags == -1)
					{
						item->ItemFlags[2] = 0;
					}
					else if (item->TriggerFlags == -3)
					{
						item->ItemFlags[2] = 0;
					}
				}

				item->ItemFlags[2] = 0;
			}
		}
		else
		{
			ShakeRaisingBlock(item);
			item->ItemFlags[1] -= 64;
		}

		// Update bone mutators.
		if (item->TriggerFlags > -1)
			item->Pose.Scale.y = (float)item->ItemFlags[1] / (float)BLOCK(4);

		bridge.Update(*item);
	}
}
