#include "framework.h"
#include "Objects/TR3/Trap/TunnelBorer.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/sphere.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Sound/sound.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;
using namespace TEN::Scripting::Properties;

namespace TEN::Entities::Traps
{
	// NOTES:
	// ItemFlags[0] = Vertical velocity. Positive value moves down, negative value moves up.

	constexpr auto TUNNEL_BORER_DEFAULT_HARM_DAMAGE = 75;
	constexpr auto TUNNEL_BORER_DEFAULT_SPEED = 5;

	void ControlTunnelBorer(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item) || item.Status == ITEM_DEACTIVATED)
			return;

		AnimateItem(item);

		// Determine height bounds.
		auto bounds = GameBoundingBox(&item);
		int upperFloorBound = bounds.Y2;
		int lowerCeilBound = bounds.Y1;

		// Get point collision.
		auto pointColl = GetPointCollision(item);
		int relFloorHeight = pointColl.GetFloorHeight() - item.Pose.Position.y;
		int relCeilHeight = pointColl.GetCeilingHeight() - item.Pose.Position.y;

		int verticalVel = PropertyHandler::Get(item, PropName_VerticalVelocity, TUNNEL_BORER_DEFAULT_SPEED);

		// Stop moving.
		if ((verticalVel > 0 && relFloorHeight <= upperFloorBound) ||
			(verticalVel < 0 && relCeilHeight >= lowerCeilBound))
		{
			item.Status = ITEM_DEACTIVATED;
			StopSoundEffect(SFX_TR4_ROLLING_BALL);
		}
		// Move.
		else
		{
			item.Pose.Position.y += verticalVel;

			if (pointColl.GetRoomNumber() != item.RoomNumber)
				ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

			SoundEffect(SFX_TR4_ROLLING_BALL, &item.Pose);
		}
	}

	void CollideTunnelBorer(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.ObjectNumber != ID_TUNNEL_BORER)
			return;

		// Collide with objects.
		if (item.Status == ITEM_ACTIVE)
		{
			if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
				return;

			HandleItemSphereCollision(item, *playerItem);
		}
		else if (item.Status != ITEM_INVISIBLE)
		{
			ObjectCollision(itemNumber, playerItem, coll);
		}
		
		// Damage entity.
		if (TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
		{
			DoDamage(playerItem, PropertyHandler::Get(item, PropName_Damage, TUNNEL_BORER_DEFAULT_HARM_DAMAGE));
			DoLotsOfBlood(playerItem->Pose.Position.x, playerItem->Pose.Position.y - CLICK(3), playerItem->Pose.Position.z, 4, playerItem->Pose.Orientation.y, playerItem->RoomNumber, 3);
			playerItem->TouchBits.ClearAll();

			SoundEffect(SFX_TR4_LARA_GRABFEET, &playerItem->Pose);
		}
	}
}
