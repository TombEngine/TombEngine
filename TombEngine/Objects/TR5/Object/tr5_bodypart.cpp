#include "framework.h"
#include "tr5_bodypart.h"

#include "Game/effects/effects.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "tr5_missile.h"
#include "Game/Lara/lara.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/items.h"
#include "Game/effects/Splash.h"
#include "Game/effects/tomb4fx.h"
#include "Math/Random.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Splash;
using namespace TEN::Math::Random;

constexpr int BODY_PART_LIFE = 64;
constexpr int BOUNCE_FALLSPEED = 32;
constexpr auto BODY_PART_EXPLODE_DAMAGE = 50;
constexpr auto BODY_PART_EXPLODE_DAMAGE_RANGE = CLICK(3.0f);

// TODO: Remove LaraItem global - TokyoSU: 12/6/2023
static void BodyPartExplode(FX_INFO& fx)
{
	TriggerExplosionSparks(fx.pos.Position.x, fx.pos.Position.y, fx.pos.Position.z, 3, -2, 0, fx.roomNumber);
	TriggerExplosionSparks(fx.pos.Position.x, fx.pos.Position.y, fx.pos.Position.z, 3, -1, 0, fx.roomNumber);
	TriggerShockwave(&fx.pos, 48, 304, (GetRandomControl() & 0x1F) + 112, 128, 32, 32, 32, EulerAngles(ANGLE(12.0f), 0, 0), 0, true, false, false, (int)ShockwaveStyle::Normal);
	
	if (ItemNearLara(fx.pos.Position, BODY_PART_EXPLODE_DAMAGE_RANGE))
		DoDamage(LaraItem, BODY_PART_EXPLODE_DAMAGE);
}

void ControlBodyPart(short fxNumber)
{
	FX_INFO* fx = &EffectList[fxNumber];
	int x = fx->pos.Position.x;
	int y = fx->pos.Position.y;
	int z = fx->pos.Position.z;

	if (fx->counter <= 0)
	{
		if (fx->speed)
			fx->pos.Orientation.x += 4 * fx->fallspeed;

		fx->fallspeed += g_GameFlow->GetSettings()->Physics.Gravity;
	}
	else
	{
		int modulus = 62 - fx->counter;
		int random = modulus <= 1 ? 0 : 2 * GetRandomControl() % modulus;
		if (fxNumber & 1)
		{
			fx->pos.Orientation.z -= random;
			fx->pos.Orientation.x += random;
		}
		else
		{
			fx->pos.Orientation.z += random;
			fx->pos.Orientation.x -= random;
		}

		if (--fx->counter < 8)
			fx->fallspeed += 2;
	}

	int fallspeed = TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx->roomNumber) ? fx->fallspeed / 4 : fx->fallspeed;
	int speed = TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx->roomNumber) ? fx->speed / 4 : fx->speed;

	fx->pos.Position.x += speed * phd_sin(fx->pos.Orientation.y);
	fx->pos.Position.y += fallspeed;
	fx->pos.Position.z += speed * phd_cos(fx->pos.Orientation.y);

	if ((fx->flag2 & BODY_DO_EXPLOSION) && !(fx->flag2 & BODY_NO_FLAME) &&
		!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx->roomNumber))
	{
		if (GenerateInt(0, 10) > (abs(fx->fallspeed) > 0 ? 5 : 8))
			TriggerFireFlame(fx->pos.Position.x, fx->pos.Position.y, fx->pos.Position.z, FlameType::Medium);
	}

	auto pointColl = GetPointCollision(fx->pos.Position, fx->roomNumber);

	if (!fx->counter)
	{
		if (fx->pos.Position.y < pointColl.GetCeilingHeight())
		{
			fx->pos.Position.y = pointColl.GetCeilingHeight();
			fx->fallspeed = -fx->fallspeed;
			fx->speed -= (fx->speed / 8);
		}

		if (fx->pos.Position.y >= pointColl.GetFloorHeight())
		{
			if (fx->flag2 & BODY_NO_BOUNCE)
			{
				fx->pos.Position.x = x;
				fx->pos.Position.y = y;
				fx->pos.Position.z = z;

				if (!(fx->flag2 & BODY_NO_SHATTER_EFFECT))
				{
					if (fx->flag2 & BODY_NO_BOUNCE_ALT)
					{
						ExplodeFX(fx, -2, 32);
					}
					else
					{
						ExplodeFX(fx, -1, 32);
					}
				}
				
				// Remove if touched floor (no bounce mode).
				if (fx->flag2 & BODY_PART_EXPLODE)
					BodyPartExplode(*fx);

				KillEffect(fxNumber);

				if (fx->flag2 & BODY_STONE_SOUND)
					SoundEffect(SFX_TR4_ROCK_FALL_LAND, &fx->pos);

				return;
			}

			if (y <= pointColl.GetFloorHeight())
			{
				// Remove if touched floor (no bounce mode).
				if (fx->flag2 & BODY_PART_EXPLODE)
				{
					BodyPartExplode(*fx);
					KillEffect(fxNumber);
				}

				if (fx->fallspeed <= BOUNCE_FALLSPEED)
				{
					fx->fallspeed = 0;
				}
				else
				{
					fx->fallspeed = -fx->fallspeed / 4;

					if (!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx->roomNumber))
					{
						if (fx->flag2 & BODY_STONE_SOUND)
						{
							SoundEffect(SFX_TR4_ROCK_FALL_LAND, &fx->pos);
						}
						else if (fx->flag2 & BODY_GIBS)
						{
							SoundEffect(SFX_TR4_LARA_THUD, &fx->pos, SoundEnvironment::Land, GenerateFloat(0.8f, 1.2f));
						}
					}
				}
			}
			else
			{
				fx->pos.Orientation.y += -ANGLE(180);
				fx->pos.Position.x = x;
				fx->pos.Position.z = z;
			}

			fx->speed -= (fx->speed / 4);
			if (abs(fx->speed) < 4)
				fx->speed = 0;

			fx->pos.Position.y = y;
		}

		if (!fx->speed && ++fx->flag1 > BODY_PART_LIFE)
		{
			if (!(fx->flag2 & BODY_NO_SMOKE))
			{
				for (int i = 0; i < 6; i++)
				{
					TriggerFlashSmoke(
						fx->pos.Position.x + GenerateInt(-16, 16),
						fx->pos.Position.y + GenerateInt(16, 32),
						fx->pos.Position.z + GenerateInt(-16, 16), fx->roomNumber);
				}
			}

			if (fx->flag2 & BODY_PART_EXPLODE)
				BodyPartExplode(*fx);

			if (!(fx->flag2 & BODY_NO_SHATTER_EFFECT))
				ExplodeFX(fx, -1, 32);

			KillEffect(fxNumber);
			return;
		}

		if ((fx->flag2 & BODY_GIBS) && (GetRandomControl() & 1))
		{
			for (int i = 0; i < (TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx->roomNumber) ? 1 : 6); i++)
			{
				DoBloodSplat(
					(GetRandomControl() & 0x3F) + fx->pos.Position.x - 32,
					(GetRandomControl() & 0x1F) + fx->pos.Position.y - 16,
					(GetRandomControl() & 0x3F) + fx->pos.Position.z - 32,
					1,
					2 * GetRandomControl(),
					fx->roomNumber);
			}
		}
	}

	if (pointColl.GetRoomNumber() != fx->roomNumber)
	{
		if (TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, pointColl.GetRoomNumber()) &&
			!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx->roomNumber))
		{
			int waterHeight = GetPointCollision(fx->pos.Position, pointColl.GetRoomNumber()).GetWaterTopHeight();

			SplashSetup.Position = Vector3(fx->pos.Position.x, waterHeight - 1, fx->pos.Position.z);
			SplashSetup.SplashPower = fx->fallspeed;
			SplashSetup.InnerRadius = 48;
			SetupSplash(&SplashSetup, pointColl.GetRoomNumber());

			// Remove if touched water.
			if (fx->flag2 & BODY_PART_EXPLODE)
			{
				BodyPartExplode(*fx);
				KillEffect(fxNumber);
			}
		}

		EffectNewRoom(fxNumber, pointColl.GetRoomNumber());
	}
}