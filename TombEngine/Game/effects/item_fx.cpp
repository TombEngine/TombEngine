#include "framework.h"
#include "Game/effects/item_fx.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/effects/smoke.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Smoke;

namespace TEN::Effects::Items
{
	void ItemBurn(ItemInfo* item, int timeout)
	{
		item->Effect.Type = EffectType::Fire;
		item->Effect.Count = timeout;
		item->Effect.LightColor = Vector3(0.8f, 0.5f, 0.0f);
		item->Effect.PrimaryEffectColor = Vector3::Zero;
		item->Effect.SecondaryEffectColor = Vector3::Zero;
	}

	void ItemCustomBurn(ItemInfo* item, const Vector3& color1, const Vector3& color2, int timeout)
	{
		item->Effect.Type = EffectType::Custom;
		item->Effect.Count = timeout;
		item->Effect.LightColor = color1;
		item->Effect.PrimaryEffectColor = color1;
		item->Effect.SecondaryEffectColor = color2;
	}

	void ItemElectricBurn(ItemInfo* item, int timeout)
	{
		item->Effect.Type = EffectType::Sparks;
		item->Effect.Count = timeout;
		item->Effect.LightColor = Vector3(0.0f, 0.6f, 1.0f);
	}

	void ItemBlueElectricBurn(ItemInfo* item, int timeout)
	{
		item->Effect.Type = EffectType::ElectricIgnite;
		item->Effect.Count = timeout / FPS;
		item->Effect.LightColor = Vector3(0.0f, 0.6f, 1.0f);
	}

	void ItemRedLaserBurn(ItemInfo* item, int timeout)
	{
		item->Effect.Type = EffectType::RedIgnite;
		item->Effect.Count = timeout / FPS;
		item->Effect.LightColor = Vector3(1.0f, 0.4f, 0.0f);
	}

	void ItemSmoke(ItemInfo* item, int timeout)
	{
		item->Effect.Type = EffectType::Smoke;
		item->Effect.Count = timeout;
	}

	void LavaBurn(ItemInfo* item)
	{
		if (item->IsLara() && GetLaraInfo(item)->Control.WaterStatus == WaterStatus::FlyCheat)
			return;

		if (item->HitPoints < 0)
			return;

		auto height = GetPointCollision(Vector3i(item->Pose.Position.x, 32000, item->Pose.Position.z), item->RoomNumber).GetFloorHeight();
		if (item->Floor == height)
		{
			item->HitPoints = -1;
			item->HitStatus = true;
			ItemBurn(item);
		}
	}

	void LaraBreath(ItemInfo* item)
	{
		if (!item->IsLara())
			return;

		const auto& player = GetLaraInfo(*item);

		if (player.Control.WaterStatus == WaterStatus::Underwater || item->HitPoints <= 0)
			return;

		if (!TestEnvironment(ENV_FLAG_COLD, item))
			return;

		switch (item->Animation.AnimNumber)
		{
		case LA_STAND_IDLE:
		case LA_CROUCH_IDLE:
		case LA_CRAWL_IDLE:
			if (item->Animation.FrameNumber < 30)
				return;
			break;

		default:
			if (Wibble < 0x80 || Wibble > 0xC0)
				return;
		}

		auto offset = GetJointPosition(item, LM_HEAD, Vector3i(0, -4, 64));
		TriggerBreathSmoke(offset.x, offset.y, offset.z, item->Pose.Orientation.y);
	}
}
