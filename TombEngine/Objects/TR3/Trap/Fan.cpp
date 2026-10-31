#include "framework.h"
#include "Objects/TR3/Trap/Fan.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Scripting::Properties;
using namespace TEN::Collision::Sphere;
using namespace TEN::Collision::Point;

namespace TEN::Entities::Traps
{
	constexpr auto FAN_HARM_DAMAGE = 5000;
	constexpr auto FAN_HARM_MESH = 0x2;

	enum FanState
	{
		FAN_STATE_ROTATING = 0,
		FAN_STATE_IDLE = 1,
	};

	enum FanAnim
	{
		FAN_ANIM_ROTATING = 0,
		FAN_ANIM_STOPPING = 1,
		FAN_ANIM_IDLE = 2,
		FAN_ANIM_STARTING = 3,
	};

	void InitializeFan(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.ItemFlags[0] = FAN_HARM_MESH;
	}

	void ControlFan(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TriggerActive(&item))
		{
			if (item.Animation.TargetState != FAN_STATE_ROTATING)
			{
				item.Animation.TargetState = FAN_STATE_ROTATING;
				item.Status = ITEM_ACTIVE;
			}

			item.ItemFlags[0] = FAN_HARM_MESH;
		}
		else
		{
			if (item.Animation.TargetState != FAN_STATE_IDLE)
			{
				item.Animation.TargetState = FAN_STATE_IDLE;
			}
			else
			{
				if (item.Animation.AnimNumber == FAN_ANIM_IDLE &&
					item.Animation.FrameNumber == GetAnimData(item).EndFrameNumber)
				{
					item.Status = ITEM_NOT_ACTIVE;
				}
			}
		}

		item.ItemFlags[3] = PropertyHandler::Get(item, PropName_Damage, FAN_HARM_DAMAGE);

		AnimateItem(&item);
	}

	void CollideFan(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];
		
		// Deactivate collision when the fan is not active
		if (!TriggerActive(&item))
			return;
		
		GenericSphereBoxCollision(itemNumber, playerItem, coll);
	}
}
