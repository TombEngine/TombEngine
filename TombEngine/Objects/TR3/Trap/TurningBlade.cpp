#include "framework.h"
#include "Objects/TR3/Trap/TurningBlade.h"

#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Collision::Point;

namespace TEN::Entities::Traps
{
	constexpr auto TURNING_BLADE_HARM_DAMAGE = 100;

	const std::vector<unsigned int> TurningBladeHarmJoints = { 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	const std::vector<Vector3i> TurningBladeBounds =
	{
		Vector3i(-CLICK(2), CLICK(2), -BLOCK(2)),
		Vector3i(-896, -96, 96),
		Vector3i(-CLICK(2), CLICK(2), -128),
		Vector3i(0, -96, 96),
		Vector3i(-CLICK(2), -384, -BLOCK(2)),
		Vector3i(0, -96, 96),
		Vector3i(384, CLICK(2), -BLOCK(2)),
		Vector3i(0, -96, 96)
	};

	void ControlTurningBlade(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		AnimateItem(&item);	
	}

	void CollideTurningBlade(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (item->Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(item, laraItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(*item, *laraItem))
			return;

		// Blades deal damage cumulatively.
		auto spheres = item->GetSpheres();
		for (int i = 0; i < TurningBladeHarmJoints.size(); i++)
		{
			if (item->TouchBits.Test(TurningBladeHarmJoints[i]))
			{
				DoDamage(laraItem, TURNING_BLADE_HARM_DAMAGE);
				DoBloodSplat(
					(GetRandomControl() & 0x3F) + laraItem->Pose.Position.x - 32,
					(GetRandomControl() & 0x1F) + spheres[i].Center.y - 16,
					(GetRandomControl() & 0x3F) + laraItem->Pose.Position.z - 32,
					(GetRandomControl() & 3) + 2,
					GetRandomControl() * 2,
					laraItem->RoomNumber);

				TriggerLaraBlood();

				if (laraItem->HitPoints > 0)
				{
					ItemPushItem(item, laraItem, coll, false, 1);
				}
			}
		}
	}
}
