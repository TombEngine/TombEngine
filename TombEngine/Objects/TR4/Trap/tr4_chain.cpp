#include "framework.h"
#include "Objects/TR4/Trap/tr4_chain.h"

#include "Game/animation.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Specific/level.h"

#include "Objects/TR5/Trap/LaserBeam.h"
#include "Specific/level.h"

namespace TEN::Entities::Traps
{
	void ControlChain(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.TriggerFlags)
		{
			item.ItemFlags[2] = 1;
			item.ItemFlags[3] = 75;

			if (TriggerActive(&item))
			{
				*(int*)&item.ItemFlags[0] = 0x787E;
				AnimateItem(&item);
				return;
			}
		}
		else
		{
			item.ItemFlags[3] = 25;

			if (TriggerActive(&item))
			{
				*(int*)&item.ItemFlags[0] = 0x780;
				AnimateItem(&item);

				auto pos1 = GetJointPosition(item, 5, Vector3i(38, -45, 0));
				auto pos2 = GetJointPosition(item, 5, Vector3i(382, -45,0));

				auto orient = Geometry::GetOrientToPoint(pos1.ToVector3(), pos2.ToVector3());
				//auto orient = Geometry::GetOrientToPoint(origin, target)

				TEN::Entities::Traps::EmitTransientLaserBeam(GameVector(pos1, item.RoomNumber), orient, 4, Vector4(0.0f, 1.0f, 0.0f, 1.0f), true, true, true);
				return;
			}
		}

		*(int*)&item.ItemFlags[0] = 0;
	}
}
