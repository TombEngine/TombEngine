#include "framework.h"
#include "Objects/TR5/Object/tr5_twoblockplatform.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Math;
using namespace TEN::Renderer;

namespace TEN::Entities::Generic
{
	static std::optional<int> GetTwoBlockPlatformFloorHeight(const ItemInfo& item, const Vector3i& pos)
	{
		if (!item.MeshBits.TestAny())
			return std::nullopt;

		return GetBridgeItemIntersect(item, pos, false);
	}

	static std::optional<int> GetTwoBlockPlatformCeilingHeight(const ItemInfo& item, const Vector3i& pos)
	{
		if (!item.MeshBits.TestAny())
			return std::nullopt;

		return GetBridgeItemIntersect(item, pos, true);
	}

	static int GetTwoBlockPlatformFloorBorder(const ItemInfo& item)
	{
		return GetBridgeBorder(item, false);
	}

	static int GetTwoBlockPlatformCeilingBorder(const ItemInfo& item)
	{
		return GetBridgeBorder(item, true);
	}

	void InitializeTwoBlockPlatform(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Data = BridgeObject();
		auto& bridge = GetBridgeObject(item);

		item.ItemFlags[0] = item.Pose.Position.y;
		item.ItemFlags[1] = 1;

		bridge.GetFloorHeight = GetTwoBlockPlatformFloorHeight;
		bridge.GetCeilingHeight = GetTwoBlockPlatformCeilingHeight;
		bridge.GetFloorBorder = GetTwoBlockPlatformFloorBorder;
		bridge.GetCeilingBorder = GetTwoBlockPlatformCeilingBorder;
		bridge.Initialize(item);
	}

	void TwoBlockPlatformControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		auto& bridge = GetBridgeObject(*item);

		if (TriggerActive(item))
		{
			if (item->TriggerFlags)
			{
				int targetHeight = (item->ItemFlags[0] - 16 * int(item->TriggerFlags & 0xFFFFFFF0));
				int vel = item->TriggerFlags & 0xF;

				if (item->Pose.Position.y > targetHeight)
				{
					item->Pose.Position.y -= vel;
				}
				else
				{
					return;
				}

				// @BRIDGEME
				int distToPortal = *&g_Level.Rooms[item->RoomNumber].TopHeight - item->Pose.Position.y;
				if (distToPortal <= vel)
					bridge.Update(*item);

				// HACK: Must probe slightly higher to avoid strange bug where the room number sometimes isn't
				// updated when the platform crosses room boundaries. -- Sezz 2025.01.18
				// TODO: Maybe not necessary anymore after bridge refactors.
				auto pointColl = GetPointCollision(*item, 0, 0, -CLICK(0.5f));

				item->Floor = pointColl.GetFloorHeight();

				if (pointColl.GetRoomNumber() != item->RoomNumber)
				{
					bridge.Disable(*item);
					ItemNewRoom(itemNumber, pointColl.GetRoomNumber());
					bridge.Enable(*item);
				}
			}
			else
			{
				bool onObject = false;

				int height = LaraItem->Pose.Position.y + 1;
				if (GetBridgeItemIntersect(*item, LaraItem->Pose.Position, false).has_value())
				{
					if (LaraItem->Pose.Position.y <= item->Pose.Position.y + 32)
					{
						if (item->Pose.Position.y < height)
							onObject = true;
					}
				}

				if (onObject && LaraItem->Animation.AnimNumber != LA_HOP_BACK_CONTINUE)
				{
					item->ItemFlags[1] = 1;
				}
				else
				{
					item->ItemFlags[1] = -1;
				}

				if (item->ItemFlags[1] < 0)
				{
					if (item->Pose.Position.y <= item->ItemFlags[0])
					{
						item->ItemFlags[1] = 1;
					}
					else
					{
						SoundEffect(SFX_TR4_RAISING_BLOCK_2, &item->Pose);
						item->Pose.Position.y -= 4;
					}
				}
				else if (item->ItemFlags[1] > 0)
				{
					if (item->Pose.Position.y >= item->ItemFlags[0] + 128)
					{
						item->ItemFlags[1] = -1;
					}
					else
					{
						SoundEffect(SFX_TR4_RAISING_BLOCK_2, &item->Pose);
						item->Pose.Position.y += 4;
					}
				}
			}
		}

		bridge.Update(*item);
	}
}
