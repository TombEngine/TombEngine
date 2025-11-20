#include "framework.h"
#include "Game/Lara/Context/Context.h"

#include "Game/animation.h"
#include "Game/collision/Attractor.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/floordata.h"
#include "Game/control/los.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_tests.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"

using namespace TEN::Collision::Attractor;
using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Input;

namespace TEN::Player
{
	PlayerAttractorData::~PlayerAttractorData()
	{
		// TODO: Polymorphism to avoid global.
		Detach(*LaraItem);
	}

	void PlayerAttractorData::Attach(ItemInfo& playerItem, AttractorObject& attrac, float chainDist,
									 const Vector3& relPosOffset, const EulerAngles& relOrientOffset,
									 const Vector3& relDeltaPos, const EulerAngles& relDeltaOrient)
	{
		Detach(playerItem);

		Attractor = &attrac;
		PathDistance = chainDist;
		RelPosOffset = relPosOffset;
		RelOrientOffset = relOrientOffset;
		RelDeltaPos = relDeltaPos;
		RelDeltaOrient = relDeltaOrient;

		Attractor->AttachItem(playerItem);
	}

	void PlayerAttractorData::Detach(ItemInfo& playerItem)
	{
		if (Attractor == nullptr)
			return;

		Attractor->DetachItem(playerItem);
		*this = {};
	};

	void PlayerAttractorData::Update(ItemInfo& playerItem, AttractorObject& attrac, float chainDist,
									 const Vector3& relPosOffset, const EulerAngles& relOrientOffset)
	{
		Attach(
			playerItem, attrac, chainDist,
			relPosOffset, relOrientOffset,
			RelDeltaPos, RelDeltaOrient);
	}

	PlayerContext::PlayerContext(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		/*Item = &item;
		Player = &player;
		Collision = &coll;*/
	}

	bool CanSlide(const ItemInfo& item, const CollisionInfo& coll)
	{
		constexpr auto ABS_FLOOR_BOUND = STEPUP_HEIGHT;

		const auto& player = GetLaraInfo(item);

		// 1) Test player water status.
		if (player.Control.WaterStatus == WaterStatus::Wade)
			return false;

		// Get point collision.
		auto pointColl = GetPointCollision(item, 0, 0, -coll.Setup.Height / 2); // NOTE: Offset required for correct bridge collision.
		int relFloorHeight = pointColl.GetFloorHeight() - item.Pose.Position.y;

		// 2) Assess point collision.
		if (abs(relFloorHeight) <= ABS_FLOOR_BOUND && // Floor height is within upper/lower floor bounds.
			pointColl.IsSteepFloor())			  // Floor is a slippery slope.
		{
			return true;
		}

		return false;
	}

	bool CanSteerOnSlide(const ItemInfo& item, const CollisionInfo& coll)
	{
		return g_GameFlow->GetSettings()->Animations.SlideExtended;
	}

	bool CanDismountTightrope(const ItemInfo& item, const CollisionInfo& coll)
	{
		const auto& player = GetLaraInfo(item);

		auto pointColl = GetPointCollision(item);

		if (player.Control.Tightrope.CanDismount &&			  // Dismount is allowed.
			pointColl.GetFloorHeight() == item.Pose.Position.y) // Floor is level with player.
		{
			return true;
		}

		return false;
	}
}
