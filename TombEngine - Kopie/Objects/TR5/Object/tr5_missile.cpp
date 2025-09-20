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

void MissileControl(short itemNumber)
{
	auto* fx = &EffectList[itemNumber];

	if (fx->flag1 == 2)
	{
		fx->pos.Orientation.z += 16 * fx->speed;

		if (fx->speed > 64)
			fx->speed -= 4;

		if (fx->pos.Orientation.x > -12288)
		{
			if (fx->fallspeed < 512)
				fx->fallspeed += 36;
			fx->pos.Orientation.x -= fx->fallspeed;
		}
	}
	else
	{
		auto orient = Geometry::GetOrientToPoint(
			Vector3(fx->pos.Position.x, fx->pos.Position.y + CLICK(1), fx->pos.Position.z),
			LaraItem->Pose.Position.ToVector3());

		int dh;
		if (fx->flag1)
			dh = fx->flag1 != 1 ? 768 : 384;
		else
		{
			if (fx->counter)
				fx->counter--;

			dh = 256;
		}

		if (fx->speed < 192)
		{
			if (fx->flag1 == 0 || fx->flag1 == 1)
				fx->speed++;

			int dy = orient.y - fx->pos.Orientation.y;
			if (abs(dy) > abs(ANGLE(180.0f)))
				dy = -dy;
			dy /= 8;

			int dx = orient.x - fx->pos.Orientation.x;
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

			fx->pos.Orientation.y += dy;
			fx->pos.Orientation.x += dx;
		}
		
		fx->pos.Orientation.z += 16 * fx->speed;

		if (!fx->flag1)
			fx->pos.Orientation.z += 16 * fx->speed;
	}

	int x = fx->pos.Position.x;
	int y = fx->pos.Position.y;
	int z = fx->pos.Position.z;

	int c = fx->speed * phd_cos(fx->pos.Orientation.x);

	fx->pos.Position.x += c * phd_sin(fx->pos.Orientation.y);
	fx->pos.Position.y += fx->speed * phd_sin(-fx->pos.Orientation.x);
	fx->pos.Position.z += c * phd_cos(fx->pos.Orientation.y);

	auto probe = GetPointCollision(fx->pos.Position, fx->roomNumber);
	
	if (fx->pos.Position.y >= probe.GetFloorHeight() || fx->pos.Position.y <= probe.GetCeilingHeight())
	{
		fx->pos.Position.x = x;
		fx->pos.Position.y = y;
		fx->pos.Position.z = z;

		if (fx->flag1)
		{
			if (fx->flag1 == 1)
			{
				TriggerExplosionSparks(x, y, z, 3, -2, 2, fx->roomNumber);
				fx->pos.Position.y -= 64;
				TriggerShockwave(&fx->pos, 48, 256, 64, 64, 128, 0, 24, EulerAngles::Identity, 1, true, false, false, (int)ShockwaveStyle::Normal);
				fx->pos.Position.y -= 128;
				TriggerShockwave(&fx->pos, 48, 256, 48, 64, 128, 0, 24, EulerAngles::Identity, 1, true, false, false, (int)ShockwaveStyle::Normal);
			}
			else if (fx->flag1 == 2)
			{
				ExplodeFX(fx, 0, 32);
				SoundEffect(251, &fx->pos);
			}
		}
		else
		{
			TriggerExplosionSparks(x, y, z, 3, -2, 0, fx->roomNumber);
			TriggerShockwave(&fx->pos, 48, 240, 48, 128, 64, 0, 24, EulerAngles::Identity, 2, true, false, false, (int)ShockwaveStyle::Normal);
		}
		
		KillEffect(itemNumber);
	}
	else if (ItemNearLara(fx->pos.Position, 200))
	{
		if (fx->flag1)
		{
			if (fx->flag1 == 1)
			{
				// ROMAN_GOD hit effect
				TriggerExplosionSparks(x, y, z, 3, -2, 2, fx->roomNumber);
				fx->pos.Position.y -= 64;
				TriggerShockwave(&fx->pos, 48, 256, 64, 0, 128, 64, 24, EulerAngles::Identity, 1, true, false, false, (int)ShockwaveStyle::Normal);
				fx->pos.Position.y -= 128;
				TriggerShockwave(&fx->pos, 48, 256, 48, 0, 128, 64, 24, EulerAngles::Identity, 1, true, false, false, (int)ShockwaveStyle::Normal);
				KillEffect(itemNumber);
				DoDamage(LaraItem, 200);
			}
			else
			{
				if (fx->flag1 == 2)
				{
					// IMP hit effect
					ExplodeFX(fx, 0, 32);
					DoDamage(LaraItem, 50);
					DoBloodSplat(fx->pos.Position.x, fx->pos.Position.y, fx->pos.Position.z, (GetRandomControl() & 3) + 2, LaraItem->Pose.Orientation.y, LaraItem->RoomNumber);
					SoundEffect(SFX_TR5_IMP_STONE_HIT, &fx->pos);
					SoundEffect(SFX_TR4_LARA_INJURY, &LaraItem->Pose);
				}
				
				KillEffect(itemNumber);
			}
		}
		else
		{
			// HYDRA hit effect
			TriggerExplosionSparks(x, y, z, 3, -2, 0, fx->roomNumber);
			TriggerShockwave(&fx->pos, 48, 240, 48, 128, 96, 0, 24, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
			if (LaraItem->HitPoints >= 500)
				DoDamage(LaraItem, 300);
			else
				ItemBurn(LaraItem);
			KillEffect(itemNumber);
		}
	}
	else
	{
		if (probe.GetRoomNumber() != fx->roomNumber)
			EffectNewRoom(itemNumber, probe.GetRoomNumber());

		if (GlobalCounter & 1)
		{
			auto pos = Vector3i(x, y, z);
			int xv = x - fx->pos.Position.x;
			int yv = y - fx->pos.Position.y;
			int zv = z - fx->pos.Position.z;

			if (fx->flag1 == 1)
				TriggerRomanStatueMissileSparks(&pos, itemNumber);
			else
			{
				TriggerHydraMissileSparks(&pos, 4 * xv, 4 * yv, 4 * zv);
				TriggerHydraMissileSparks(&fx->pos.Position, 4 * xv, 4 * yv, 4 * zv);
			}
		}
	}
}

void ExplodeFX(FX_INFO* fx, int noXZVel, int bits)
{
	ShatterItem.yRot = fx->pos.Orientation.y;
	ShatterItem.meshIndex = fx->frameNumber;
	ShatterItem.color = Vector4::One;
	ShatterItem.sphere.Center = fx->pos.Position.ToVector3();
	ShatterItem.bit = 0;
	ShatterItem.flags = fx->flag2 & 0x1400;

	if (fx->flag2 & 0x2000)
		DebrisFlags = 1;

	ShatterObject(&ShatterItem, 0, bits, fx->roomNumber, noXZVel);

	DebrisFlags = 0;
}
