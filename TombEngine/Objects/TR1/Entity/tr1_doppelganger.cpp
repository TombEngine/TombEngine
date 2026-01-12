#include "framework.h"
#include "Objects/TR1/Entity/tr1_doppelganger.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/misc.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;

namespace TEN::Entities::Creatures::TR1
{
	constexpr int DOPPELGANGER_BURN_TIMEOUT = 5 * FPS;

	enum class DoppelgangerFallState
	{
		None,
		Fall,
		Death
	};

	const ItemInfo* FindDoppelgangerReferenceItem(const ItemInfo& item, GAME_OBJECT_ID objectID)
	{
		for (int i = 0; i < g_Level.NumItems; i++)
		{
			const auto& currentItem = g_Level.Items[i];
			if (currentItem.ObjectNumber == objectID && item.TriggerFlags == currentItem.TriggerFlags)
				return &currentItem;
		}

		return nullptr;
	}

	static int GetWeaponDamage(LaraWeaponType weaponType)
	{
		return (Weapons[(int)weaponType].Damage * 10);
	}

	void DoppelgangerControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];

		const auto* referenceItem = FindDoppelgangerReferenceItem(item, ID_DOPPELGANGER_ORIGIN);
		if (referenceItem == nullptr)
		{
			TENLog("Doppelganger requires ID_DOPPELGANGER_ORIGIN placed in room center.", LogLevel::Warning);
			return;
		}

		switch ((DoppelgangerFallState)item.ItemFlags[7])
		{
		case DoppelgangerFallState::None:
		{
			if (item.HitPoints < LARA_HEALTH_MAX)
			{
				item.HitPoints = LARA_HEALTH_MAX;
				DoDamage(LaraItem, GetWeaponDamage(Lara.Control.Weapon.GunType));
			}

			int playerFloorHeight = GetPointCollision(*LaraItem).GetFloorHeight();

			// Get floor heights for comparison.
			auto pos = Vector3i(
				(referenceItem->Pose.Position.x * 2) - LaraItem->Pose.Position.x,
				LaraItem->Pose.Position.y,
				(referenceItem->Pose.Position.z * 2) - LaraItem->Pose.Position.z);
			auto orient = LaraItem->Pose.Orientation + EulerAngles(0, ANGLE(180.0f), 0);

			item.Floor = GetPointCollision(pos, item.RoomNumber).GetFloorHeight();

			// Mirror player.
			SetAnimation(item, LaraItem->Animation.AnimObjectID, LaraItem->Animation.AnimNumber, LaraItem->Animation.FrameNumber);
			item.Animation.IsAirborne = LaraItem->Animation.IsAirborne;
			item.Pose.Position = pos;
			item.Pose.Orientation = orient;

			// Compare floor heights.
			if (item.Floor >= (playerFloorHeight + (BLOCK(1) + 1)) && !LaraItem->Animation.IsAirborne)
			{
				item.Pose.Position.y += CLICK(0.5f);

				SetAnimation(item, LaraItem->Animation.AnimObjectID, LA_FREEFALL);
				item.Animation.IsAirborne = true;
				item.Animation.Velocity = Vector3::Zero;
				item.ItemFlags[7] = (short)DoppelgangerFallState::Fall;
			}

			break;
		}

		case DoppelgangerFallState::Fall:
		{
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

			auto pointColl = GetPointCollision(item);
			item.Floor = pointColl.GetFloorHeight();

			if (item.Pose.Position.y >= item.Floor)
			{
				if (item.Animation.AnimNumber != LA_FREEFALL_DEATH)
				{
					item.Pose.Position = GetNearestSectorCenter(item.Pose.Position);
					item.Pose.Position.y = item.Floor;

					TestTriggers(&item, true);

					if (pointColl.GetBottomSector().Flags.Death)
						item.Effect.Type = EffectType::Fire;

					SetAnimation(item, LaraItem->Animation.AnimObjectID, LA_FREEFALL_DEATH);
					item.Animation.IsAirborne = false;
					item.Animation.Velocity.y = 0.0f;
				}

				const auto& anim = GetAnimData(item);
				if (item.Animation.AnimNumber == LA_FREEFALL_DEATH &&
					item.Animation.FrameNumber >= (anim.EndFrameNumber - 1))
				{
					item.ItemFlags[7] = (short)DoppelgangerFallState::Death;
				}
			}

			break;
		}

		case DoppelgangerFallState::Death:

			item.HitPoints = 0;
			item.ItemFlags[6]++;

			if (item.ItemFlags[6] > DOPPELGANGER_BURN_TIMEOUT)
			{
				DisableEntityAI(itemNumber);
				RemoveActiveItem(itemNumber);
				item.Collidable = false;
			}

			break;
		}

		ItemNewRoom(itemNumber, GetPointCollision(item).GetRoomNumber());
		AnimateItem(item);
	}
}
