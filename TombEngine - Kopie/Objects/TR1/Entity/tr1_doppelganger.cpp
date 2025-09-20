#include "framework.h"
#include "Objects/TR1/Entity/tr1_doppelganger.h"

#include "Game/animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/misc.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;

namespace TEN::Entities::Creatures::TR1
{
	ItemInfo* FindDoppelgangerReference(const ItemInfo& item, int objectNumber)
	{
		for (int i = 0; i < g_Level.NumItems; i++)
		{
			auto& currentItem = g_Level.Items[i];
			if (currentItem.ObjectNumber == objectNumber && item.TriggerFlags == currentItem.TriggerFlags)
				return &currentItem;
		}

		return nullptr;
	}

	int GetWeaponDamage(LaraWeaponType weaponType)
	{
		return (Weapons[(int)weaponType].Damage * 10);
	}

	void DoppelgangerControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		if (item.HitPoints < LARA_HEALTH_MAX)
		{
			item.HitPoints = LARA_HEALTH_MAX;
			DoDamage(LaraItem, GetWeaponDamage(Lara.Control.Weapon.GunType));
		}

		auto* referencePtr = FindDoppelgangerReference(item, ID_DOPPELGANGER_ORIGIN);
		if (referencePtr == nullptr)
		{
			TENLog("Doppelganger requires ID_DOPPELGANGER_ORIGIN to be placed in room center.", LogLevel::Warning);
			return;
		}

		switch (item.ItemFlags[7])
		{
		case 0:
		{
			int laraFloorHeight = GetPointCollision(*LaraItem).GetFloorHeight();

			// Get floor heights for comparison.
			auto pos = Vector3i(
				(referencePtr->Pose.Position.x * 2) - LaraItem->Pose.Position.x,
				LaraItem->Pose.Position.y,
				(referencePtr->Pose.Position.z * 2) - LaraItem->Pose.Position.z);
			item.Floor = GetPointCollision(pos, item.RoomNumber).GetFloorHeight();

			// Animate doppelganger, mirroring player's position.
			item.Animation.AnimNumber = LaraItem->Animation.AnimNumber;
			item.Animation.FrameNumber = LaraItem->Animation.FrameNumber;
			item.Pose.Position = pos;
			item.Pose.Orientation.x = LaraItem->Pose.Orientation.x;
			item.Pose.Orientation.y = LaraItem->Pose.Orientation.y - ANGLE(180.0f);
			item.Pose.Orientation.z = LaraItem->Pose.Orientation.z;
			item.Animation.IsAirborne = LaraItem->Animation.IsAirborne;

			// Compare floor heights.
			if (item.Floor >= (laraFloorHeight + BLOCK(1) + 1) && !LaraItem->Animation.IsAirborne)
			{
				SetAnimation(item, LA_FREEFALL);
				item.Animation.IsAirborne = true;
				item.Pose.Position.y += 64;
				item.ItemFlags[7] = 1;
			}

			break;
		}
		case 1:
			if (item.Animation.Velocity.x > 0.0f)
			{
				item.Animation.Velocity.x -= 2;
			}
			else if (item.Animation.Velocity.x < 0.0f)
			{
				item.Animation.Velocity.x += 2;
			}
			else
			{
				item.Animation.Velocity.x = 0.0f;
			}

			if (item.Animation.Velocity.z > 0.0f)
			{
				item.Animation.Velocity.z -= 2;
			}
			else if (item.Animation.Velocity.z < 0.0f)
			{
				item.Animation.Velocity.z += 2;
			}
			else
			{
				item.Animation.Velocity.z = 0.0f;
			}

			TestTriggers(&item, true);
			item.Floor = GetPointCollision(item).GetFloorHeight();

			if (item.Pose.Position.y >= item.Floor)
			{
				item.Pose.Position.y = item.Floor;
				TestTriggers(&item, true);

				SetAnimation(item, LA_FREEFALL_DEATH);
				item.Animation.IsAirborne = false;
				item.Animation.Velocity.y = 0.0f;

				if (item.Animation.FrameNumber >= GetFrameCount(LA_FREEFALL_DEATH) - 1)
					item.ItemFlags[7] = 2;
			}

			break;

		case 2:
			DisableEntityAI(itemNumber);
			RemoveActiveItem(itemNumber);
			item.Collidable = false;
			break;
		}

		ItemNewRoom(itemNumber, GetPointCollision(item).GetRoomNumber());
		AnimateItem(&item);
	}
}
