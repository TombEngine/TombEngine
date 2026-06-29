#include "framework.h"
#include "tr5_missile.h"
#include "Game/items.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/effects.h"
#include "Specific/level.h"
#include "Game/effects/debris.h"
#include "Game/Lara/lara.h"
#include "Sound/sound.h"
#include "Objects/TR5/Entity/tr5_roman_statue.h"
#include "Objects/TR5/Entity/tr5_hydra.h"
#include "Game/collision/collide_item.h"
#include "Game/effects/item_fx.h"
#include "Math/Math.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Items;
using namespace TEN::Math;

int DebrisFlags;

void MissileControl(short fxNumber)
{
	auto& fx = g_Level.Items[fxNumber];
	auto& fxInfo = GetFXInfo(fx);

	if (fxInfo.Flag1 == 2)
	{
		fx.Pose.Orientation.z += 16 * fx.Animation.Velocity.z;

		if (fx.Animation.Velocity.z > 64)
			fx.Animation.Velocity.z -= 4;

		if (fx.Pose.Orientation.x > -12288)
		{
			if (fx.Animation.Velocity.y < 512)
				fx.Animation.Velocity.y += 36;
			fx.Pose.Orientation.x -= fx.Animation.Velocity.y;
		}
	}
	else
	{
		auto orient = Geometry::GetOrientToPoint(
			Vector3(fx.Pose.Position.x, fx.Pose.Position.y + CLICK(1), fx.Pose.Position.z),
			LaraItem->Pose.Position.ToVector3());

		int dh;
		if (fxInfo.Flag1)
			dh = fxInfo.Flag1 != 1 ? 768 : 384;
		else
		{
			if (fxInfo.Counter)
				fxInfo.Counter--;

			dh = 256;
		}

		if (fx.Animation.Velocity.z < 192)
		{
			if (fxInfo.Flag1 == 0 || fxInfo.Flag1 == 1)
				fx.Animation.Velocity.z++;

			int dy = orient.y - fx.Pose.Orientation.y;
			if (abs(dy) > abs(ANGLE(180.0f)))
				dy = -dy;
			dy /= 8;

			int dx = orient.x - fx.Pose.Orientation.x;
			if (abs(dx) > abs(ANGLE(180.0f)))
				dx = -dx;
			dx /= 8;

			if (dy <= dh)
			{
				if (dy < -dh)
					dy = -dh;
			}
			else
				dy = dh;
			
			if (dx <= dh)
			{
				if (dx < -dh)
					dx = -dh;
			}
			else
				dx = dh;

			fx.Pose.Orientation.y += dy;
			fx.Pose.Orientation.x += dx;
		}
		
		fx.Pose.Orientation.z += 16 * fx.Animation.Velocity.z;

		if (!fxInfo.Flag1)
			fx.Pose.Orientation.z += 16 * fx.Animation.Velocity.z;
	}

	int x = fx.Pose.Position.x;
	int y = fx.Pose.Position.y;
	int z = fx.Pose.Position.z;

	int c = fx.Animation.Velocity.z * phd_cos(fx.Pose.Orientation.x);

	fx.Pose.Position.x += c * phd_sin(fx.Pose.Orientation.y);
	fx.Pose.Position.y += fx.Animation.Velocity.z * phd_sin(-fx.Pose.Orientation.x);
	fx.Pose.Position.z += c * phd_cos(fx.Pose.Orientation.y);

	auto probe = GetPointCollision(fx.Pose.Position, fx.RoomNumber);
	
	if (fx.Pose.Position.y >= probe.GetFloorHeight() || fx.Pose.Position.y <= probe.GetCeilingHeight())
	{
		fx.Pose.Position.x = x;
		fx.Pose.Position.y = y;
		fx.Pose.Position.z = z;

		if (fxInfo.Flag1)
		{
			if (fxInfo.Flag1 == 1)
			{
				TriggerExplosionSparks(x, y, z, 3, -2, 2, fx.RoomNumber);
				fx.Pose.Position.y -= 64;
				TriggerShockwave(&fx.Pose, 48, 256, 64, 64, 128, 0, 24, EulerAngles::Identity, 1, true, false, false, (int)ShockwaveStyle::Normal);
				fx.Pose.Position.y -= 128;
				TriggerShockwave(&fx.Pose, 48, 256, 48, 64, 128, 0, 24, EulerAngles::Identity, 1, true, false, false, (int)ShockwaveStyle::Normal);
			}
			else if (fxInfo.Flag1 == 2)
			{
				ExplodeFX(fx, 0, 32);
				SoundEffect(251, &fx.Pose);
			}
		}
		else
		{
			TriggerExplosionSparks(x, y, z, 3, -2, 0, fx.RoomNumber);
			TriggerShockwave(&fx.Pose, 48, 240, 48, 128, 64, 0, 24, EulerAngles::Identity, 2, true, false, false, (int)ShockwaveStyle::Normal);
		}
		
		KillItem(fxNumber);
	}
	else if (ItemNearLara(fx.Pose.Position, 200))
	{
		if (fxInfo.Flag1)
		{
			if (fxInfo.Flag1 == 1)
			{
				// ROMAN_GOD hit effect
				TriggerExplosionSparks(x, y, z, 3, -2, 2, fx.RoomNumber);
				fx.Pose.Position.y -= 64;
				TriggerShockwave(&fx.Pose, 48, 256, 64, 0, 128, 64, 24, EulerAngles::Identity, 1, true, false, false, (int)ShockwaveStyle::Normal);
				fx.Pose.Position.y -= 128;
				TriggerShockwave(&fx.Pose, 48, 256, 48, 0, 128, 64, 24, EulerAngles::Identity, 1, true, false, false, (int)ShockwaveStyle::Normal);
				KillItem(fxNumber);
				DoDamage(LaraItem, 200);
			}
			else
			{
				if (fxInfo.Flag1 == 2)
				{
					// IMP hit effect
					ExplodeFX(fx, 0, 32);
					DoDamage(LaraItem, 50);
					DoBloodSplat(fx.Pose.Position.x, fx.Pose.Position.y, fx.Pose.Position.z, (GetRandomControl() & 3) + 2, LaraItem->Pose.Orientation.y, LaraItem->RoomNumber);
					SoundEffect(SFX_TR5_IMP_STONE_HIT, &fx.Pose);
					SoundEffect(SFX_TR4_LARA_INJURY, &LaraItem->Pose);
				}
				
				KillItem(fxNumber);
			}
		}
		else
		{
			// HYDRA hit effect
			TriggerExplosionSparks(x, y, z, 3, -2, 0, fx.RoomNumber);
			TriggerShockwave(&fx.Pose, 48, 240, 48, 128, 96, 0, 24, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
			if (LaraItem->HitPoints >= 500)
				DoDamage(LaraItem, 300);
			else
				ItemBurn(LaraItem);
			KillItem(fxNumber);
		}
	}
	else
	{
		if (probe.GetRoomNumber() != fx.RoomNumber)
			ItemNewRoom(fxNumber, probe.GetRoomNumber());

		if (GlobalCounter & 1)
		{
			auto pos = Vector3i(x, y, z);
			int xv = x - fx.Pose.Position.x;
			int yv = y - fx.Pose.Position.y;
			int zv = z - fx.Pose.Position.z;

			if (fxInfo.Flag1 == 1)
				TriggerRomanStatueMissileSparks(&pos, fxNumber);
			else
			{
				TriggerHydraMissileSparks(&pos, 4 * xv, 4 * yv, 4 * zv);
				TriggerHydraMissileSparks(&fx.Pose.Position, 4 * xv, 4 * yv, 4 * zv);
			}
		}
	}
}

void ExplodeFX(ItemInfo& fx, int noXZVel, int bits)
{
	auto& fxInfo = GetFXInfo(fx);

	ShatterItem.yRot = fx.Pose.Orientation.y;
	ShatterItem.meshIndex = fx.Animation.FrameNumber;
	ShatterItem.color = NEUTRAL_COLOR;
	ShatterItem.sphere.Center = fx.Pose.Position.ToVector3();
	ShatterItem.bit = 0;
	ShatterItem.flags = fxInfo.Flag2 & 0x1400;

	if (fxInfo.Flag2 & 0x2000)
		DebrisFlags = 1;

	ShatterObject(&ShatterItem, 0, bits, fx.RoomNumber, noXZVel);

	DebrisFlags = 0;
}
