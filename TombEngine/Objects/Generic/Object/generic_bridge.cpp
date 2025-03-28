#include "framework.h"
#include "Objects/Generic/Object/generic_bridge.h"

#include "Game/collision/floordata.h"
#include "Objects/game_object_ids.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;

namespace TEN::Entities::Generic
{
	static int GetTiltOffset(const ItemInfo& item, const Vector3i& pos, int tiltGrade, bool isFloor)
	{
		// Calculate delta position.
		auto deltaPos = (item.Pose.Position - pos).ToVector3();

		// Calculate tilt normal.
		int sign = isFloor ? -1 : 1;
		auto rotMatrix = EulerAngles(0, item.Pose.Orientation.y, 0).ToRotationMatrix();
		auto normal = Vector3::Transform(Vector3(-tiltGrade, 1.0f, 0.0f) * sign, rotMatrix);
		normal.Normalize();

		// Calculate and return tilt offset.
		float relPlaneHeight = -((normal.x * deltaPos.x) + (normal.z * deltaPos.z)) / normal.y;
		return ((relPlaneHeight / 4) + CLICK(tiltGrade * 0.5f));
	}

	template <int tiltGrade>
	static std::optional<int> GetBridgeFloorHeight(const ItemInfo& item, const Vector3i& pos)
	{
		auto boxHeight = GetBridgeItemIntersect(item, pos, false);
		if (boxHeight.has_value() && tiltGrade != 0)
			return (*boxHeight + GetTiltOffset(item, pos, tiltGrade, true));

		return boxHeight;
	}

	template <int tiltGrade>
	static std::optional<int> GetBridgeCeilingHeight(const ItemInfo& item, const Vector3i& pos)
	{
		auto boxHeight = GetBridgeItemIntersect(item, pos, true);
		if (boxHeight.has_value() && tiltGrade != 0)
			return ((item.Pose.Position.y + GetTiltOffset(item, pos, tiltGrade, false)) + CLICK(1));

		return boxHeight;
	}

	template <int tiltGrade>
	static int GetBridgeFloorBorder(const ItemInfo& item)
	{
		return GetBridgeBorder(item, false);
	}

	template <int tiltGrade>
	static int GetBridgeCeilingBorder(const ItemInfo& item)
	{
		return GetBridgeBorder(item, true);
	}

	void InitializeBridge(short itemNumber)
	{
		auto& bridgeItem = g_Level.Items[itemNumber];
		bridgeItem.Data = BridgeObject();
		auto& bridge = GetBridgeObject(bridgeItem);

		bridgeItem.Status = ItemStatus::ITEM_ACTIVE;
		AddActiveItem(itemNumber);

		switch (bridgeItem.ObjectNumber)
		{
		default:
		case ID_BRIDGE_FLAT:
			bridge.GetFloorHeight = GetBridgeFloorHeight<0>;
			bridge.GetCeilingHeight = GetBridgeCeilingHeight<0>;
			bridge.GetFloorBorder = GetBridgeFloorBorder<0>;
			bridge.GetCeilingBorder = GetBridgeCeilingBorder<0>;
			break;

		case ID_BRIDGE_TILT1:
			bridge.GetFloorHeight = GetBridgeFloorHeight<1>;
			bridge.GetCeilingHeight = GetBridgeCeilingHeight<1>;
			bridge.GetFloorBorder = GetBridgeFloorBorder<1>;
			bridge.GetCeilingBorder = GetBridgeCeilingBorder<1>;
			break;

		case ID_BRIDGE_TILT2:
			bridge.GetFloorHeight = GetBridgeFloorHeight<2>;
			bridge.GetCeilingHeight = GetBridgeCeilingHeight<2>;
			bridge.GetFloorBorder = GetBridgeFloorBorder<2>;
			bridge.GetCeilingBorder = GetBridgeCeilingBorder<2>;
			break;

		case ID_BRIDGE_TILT3:
			bridge.GetFloorHeight = GetBridgeFloorHeight<3>;
			bridge.GetCeilingHeight = GetBridgeCeilingHeight<3>;
			bridge.GetFloorBorder = GetBridgeFloorBorder<3>;
			bridge.GetCeilingBorder = GetBridgeCeilingBorder<3>;
			break;

		case ID_BRIDGE_TILT4:
			bridge.GetFloorHeight = GetBridgeFloorHeight<4>;
			bridge.GetCeilingHeight = GetBridgeCeilingHeight<4>;
			bridge.GetFloorBorder = GetBridgeFloorBorder<4>;
			bridge.GetCeilingBorder = GetBridgeCeilingBorder<4>;
			break;
		}

		bridge.Initialize(bridgeItem);
	}

	void ControlBridge(short itemNumber)
	{
		auto& bridgeItem = g_Level.Items[itemNumber];
		auto& bridge = GetBridgeObject(bridgeItem);

		bridge.Update(bridgeItem);
	}
}
