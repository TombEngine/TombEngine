#include "framework.h"
#include "Objects/TR5/Object/tr5_expandingplatform.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::Generic
{
	static void UpdateExpandingPlatformScale(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.Pose.Scale.z = (float)item.ItemFlags[1] / (float)BLOCK(4);
	}

	static bool IsOnExpandingPlatform(const ItemInfo& item, const Vector3i& pos)
	{
		if (item.ItemFlags[1] <= 0)
			return false;

		int xb = pos.x / BLOCK(1);
		int zb = pos.z / BLOCK(1);
		int itemxb = item.Pose.Position.x / BLOCK(1);
		int itemzb = item.Pose.Position.z / BLOCK(1);

		auto bounds = GameBoundingBox(&item);
		int halfWidth = abs(bounds.Z2 - bounds.Z1) / 2;

		if (item.Pose.Orientation.y == ANGLE(90.0f))
		{
			int xBorder = item.Pose.Position.x + halfWidth - BLOCK(1) * item.ItemFlags[1] / BLOCK(4);
			if (pos.x < xBorder || zb != itemzb || xb != itemxb)
				return false;
		}
		else if (item.Pose.Orientation.y == ANGLE(270.0f))
		{
			int xBorder = item.Pose.Position.x - halfWidth + BLOCK(1) * item.ItemFlags[1] / BLOCK(4);
			if (pos.x > xBorder || zb != itemzb || xb != itemxb)
				return false;
		}
		else if (item.Pose.Orientation.y == 0)
		{
			int zBorder = item.Pose.Position.z + halfWidth - BLOCK(1) * item.ItemFlags[1] / BLOCK(4);
			if (pos.z < zBorder || zb != itemzb || xb != itemxb)
				return false;
		}
		else if (item.Pose.Orientation.y == ANGLE(180.0f))
		{
			int zBorder = item.Pose.Position.z - halfWidth + BLOCK(1) * item.ItemFlags[1] / BLOCK(4);
			if (pos.z > zBorder || zb != itemzb || xb != itemxb)
				return false;
		}

		return GetBridgeItemIntersect(item, Vector3i(pos.x, item.Pose.Position.y, pos.z), false).has_value();
	}

	static std::optional<int> GetExpandingPlatformFloorHeight(const ItemInfo& item, const Vector3i& pos)
	{
		if (!IsOnExpandingPlatform(item, pos))
			return std::nullopt;

		return GetBridgeItemIntersect(item, pos, false);
	}

	static std::optional<int> GetExpandingPlatformCeilingHeight(const ItemInfo& item, const Vector3i& pos)
	{
		if (!IsOnExpandingPlatform(item, pos))
			return std::nullopt;

		return GetBridgeItemIntersect(item, pos, true);
	}

	static int ExpandingPlatformFloorBorder(const ItemInfo& item)
	{
		return GetBridgeBorder(item, false);
	}

	static int ExpandingPlatformCeilingBorder(const ItemInfo& item)
	{
		return GetBridgeBorder(item, true);
	}

	void InitializeExpandingPlatform(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Data = BridgeObject();
		auto& bridge = GetBridgeObject(item);

		g_Level.PathfindingBoxes[GetPointCollision(item).GetSector().PathfindingBoxID].flags &= ~BLOCKED;

		// Set scale to default.
		UpdateExpandingPlatformScale(itemNumber);

		if (item.TriggerFlags < 0)
		{
			item.AIBits |= ALL_AIOBJ;
			AddActiveItem(itemNumber);
			item.Status = ITEM_ACTIVE;
		}

		bridge.GetFloorHeight = GetExpandingPlatformFloorHeight;
		bridge.GetCeilingHeight = GetExpandingPlatformCeilingHeight;
		bridge.GetFloorBorder = ExpandingPlatformFloorBorder;
		bridge.GetCeilingBorder = ExpandingPlatformCeilingBorder;
		bridge.Initialize(item);
	}

	bool IsInFrontOfExpandingPlatform(const ItemInfo& item, const Vector3i& pos, int margin)
	{
		if (item.ItemFlags[1] <= 0)
			return false;

		int bottomHeight = GetBridgeBorder(item, true);
		int topHeight = GetBridgeBorder(item, false);

		if (LaraItem->Pose.Position.y < topHeight - 32 || LaraItem->Pose.Position.y > bottomHeight + 32)
			return false;

		auto bounds = GameBoundingBox(&item);
		auto halfWidth = abs(bounds.Z2 - bounds.Z1) / 2;

		int xb = pos.x / BLOCK(1);
		int zb = pos.z / BLOCK(1);
		int itemxb = item.Pose.Position.x / BLOCK(1);
		int itemzb = item.Pose.Position.z / BLOCK(1);

		if (item.Pose.Orientation.y == ANGLE(90.0f))
		{
			int xBorder = item.Pose.Position.x + halfWidth - margin - BLOCK(1) * item.ItemFlags[1] / BLOCK(4);
			int xBorder2 = item.Pose.Position.x + halfWidth;
			if (pos.x < xBorder || zb != itemzb || pos.x > xBorder2)
				return false;
		}
		else if (item.Pose.Orientation.y == ANGLE(270.0f))
		{
			int xBorder = item.Pose.Position.x - halfWidth + margin + BLOCK(1) * item.ItemFlags[1] / BLOCK(4);
			int xBorder2 = item.Pose.Position.x - halfWidth;
			if (pos.x > xBorder || zb != itemzb || pos.x < xBorder2)
				return false;
		}
		else if (item.Pose.Orientation.y == 0)
		{
			int zBorder = item.Pose.Position.z + halfWidth - margin - BLOCK(1) * item.ItemFlags[1] / BLOCK(4);
			int zBorder2 = item.Pose.Position.z + halfWidth;
			if (pos.z < zBorder || xb != itemxb || pos.z > zBorder2)
				return false;
		}
		else if (item.Pose.Orientation.y == ANGLE(180.0f))
		{
			int zBorder = item.Pose.Position.z - halfWidth + margin + BLOCK(1) * item.ItemFlags[1] / BLOCK(4);
			int zBorder2 = item.Pose.Position.z - halfWidth;
			if (pos.z > zBorder || xb != itemxb || pos.z < zBorder2)
				return false;
		}

		return GetBridgeItemIntersect(item, Vector3i(pos.x, item.Pose.Position.y, pos.z), false).has_value();
	}

	static void ShiftPlayerOnPlatform(const ItemInfo& item, bool isExpanding)
	{
		short angle = item.Pose.Orientation.y;
		int xShift = 0;
		int zShift = 0;

		if (item.ItemFlags[1] <= 0)
			return;

		int height = GetBridgeBorder(item, false);

		if (IsOnExpandingPlatform(item, LaraItem->Pose.Position))
		{
			// Slide player if on top of platform.
			if (LaraItem->Pose.Position.y < (height - CLICK(1 / 8.0f)) || LaraItem->Pose.Position.y > (height + CLICK(1 / 8.0f)))
				return;

			if (angle == 0)
			{
				zShift = isExpanding ? -16 : 16;
			}
			else if (angle == ANGLE(180.0f))
			{
				zShift = isExpanding ? 16 : -16;
			}
			else if (angle == ANGLE(90.0f))
			{
				xShift = isExpanding ? -16 : 16;
			}
			else if (angle == -ANGLE(90.0f))
			{
				xShift = isExpanding ? 16 : -16;
			}
		}
		else if (isExpanding && IsInFrontOfExpandingPlatform(item, LaraItem->Pose.Position, LaraCollision.Setup.Radius))
		{
			// Push player if in front of expanding platform.
			if (angle == 0)
			{
				zShift = -LaraCollision.Setup.Radius / 6;
			}
			else if (angle == ANGLE(180.0f))
			{
				zShift = LaraCollision.Setup.Radius / 6;
			}
			else if (angle == ANGLE(90.0f))
			{
				xShift = -LaraCollision.Setup.Radius / 6;
			}
			else if (angle == -ANGLE(90.0f))
			{
				xShift = LaraCollision.Setup.Radius / 6;
			}
		}

		auto* coll = &LaraCollision;
		GetCollisionInfo(coll, LaraItem, Vector3i(xShift, 0, zShift));

		if (coll->Middle.Ceiling >= 0 || coll->HitStatic)
			return;

		if (zShift != 0)
			LaraItem->Pose.Position.z += zShift;
		if (xShift != 0)
			LaraItem->Pose.Position.x += xShift;
	}

	void ControlExpandingPlatform(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		auto& bridge = GetBridgeObject(item);

		bridge.Update(item);

		if (TriggerActive(&item))
		{
			if (!item.ItemFlags[2])
				item.ItemFlags[2] = 1;

			if (item.TriggerFlags < 0)
			{
				item.ItemFlags[1] = 1;
			}
			else if (item.ItemFlags[1] < 4096)
			{
				SoundEffect(SFX_TR4_RAISING_BLOCK, &item.Pose);

				item.ItemFlags[1] += 64;
				ShiftPlayerOnPlatform(item, true);

				if (item.TriggerFlags > 0)
				{
					if (abs(item.Pose.Position.x - Camera.pos.x) < 10240 &&
						abs(item.Pose.Position.x - Camera.pos.x) < 10240 &&
						abs(item.Pose.Position.x - Camera.pos.x) < 10240)
					{
						if (item.ItemFlags[1] == 64 || item.ItemFlags[1] == 4096)
						{
							Camera.bounce = -32;
						}
						else
						{
							Camera.bounce = -16;
						}
					}
				}
			}
		}
		else if (item.ItemFlags[1] <= 0 || item.TriggerFlags < 0)
		{
			if (item.ItemFlags[2])
			{
				item.ItemFlags[1] = 0;
				item.ItemFlags[2] = 0;
			}
		}
		else
		{
			SoundEffect(SFX_TR4_RAISING_BLOCK, &item.Pose);

			if (item.TriggerFlags >= 0)
			{
				if (abs(item.Pose.Position.x - Camera.pos.x) < 10240 &&
					abs(item.Pose.Position.x - Camera.pos.x) < 10240 &&
					abs(item.Pose.Position.x - Camera.pos.x) < 10240)
				{
					if (item.ItemFlags[1] == 64 || item.ItemFlags[1] == 4096)
					{
						Camera.bounce = -32;
					}
					else
					{
						Camera.bounce = -16;
					}
				}
			}

			item.ItemFlags[1] -= 64;
			ShiftPlayerOnPlatform(item, false);
		}

		UpdateExpandingPlatformScale(itemNumber);
	}
}