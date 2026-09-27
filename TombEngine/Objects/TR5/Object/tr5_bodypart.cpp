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
#include <Specific/level.h>

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Splash;
using namespace TEN::Math::Random;

constexpr int BODY_PART_LIFE = 64;
constexpr int BOUNCE_FALLSPEED = 32;
constexpr auto BODY_PART_EXPLODE_DAMAGE = 50;
constexpr auto BODY_PART_EXPLODE_DAMAGE_RANGE = CLICK(3.0f);

// TODO: Remove LaraItem global - TokyoSU: 12/6/2023
static void BodyPartExplode(ItemInfo& fx)
{
	TriggerExplosionSparks(fx.Pose.Position.x, fx.Pose.Position.y, fx.Pose.Position.z, 3, -2, 0, fx.RoomNumber);
	TriggerExplosionSparks(fx.Pose.Position.x, fx.Pose.Position.y, fx.Pose.Position.z, 3, -1, 0, fx.RoomNumber);

	// Lift explosion effect a little to make shockwave visible on flat floors.
	auto pose = fx.Pose;
	pose.Position.y -= CLICK(0.5f);

	TriggerShockwave(&pose, 48, 304, (GetRandomControl() & 0x1F) + 112, 128, 32, 32, 32, EulerAngles::Identity, 0, true, false, false, (int)ShockwaveStyle::Normal);
	
	if (ItemNearLara(fx.Pose.Position, BODY_PART_EXPLODE_DAMAGE_RANGE))
		DoDamage(LaraItem, BODY_PART_EXPLODE_DAMAGE);
}

void ControlBodyPart(short fxNumber)
{
	auto& fx = g_Level.Items[fxNumber];
	auto& fxInfo = GetFXInfo(fx);

	int x = fx.Pose.Position.x;
	int y = fx.Pose.Position.y;
	int z = fx.Pose.Position.z;

	if (fxInfo.Counter <= 0)
	{
		if (fx.Animation.Velocity.z)
			fx.Pose.Orientation.x += 4 * fx.Animation.Velocity.y;

		fx.Animation.Velocity.y += g_GameFlow->GetSettings()->Physics.Gravity;
	}
	else
	{
		int modulus = 62 - fxInfo.Counter;
		int random = modulus <= 1 ? 0 : 2 * GetRandomControl() % modulus;
		if (fxNumber & 1)
		{
			fx.Pose.Orientation.z -= random;
			fx.Pose.Orientation.x += random;
		}
		else
		{
			fx.Pose.Orientation.z += random;
			fx.Pose.Orientation.x -= random;
		}

		if (--fxInfo.Counter < 8)
			fx.Animation.Velocity.y += 2;
	}

	int fallspeed = TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx.RoomNumber) ? fx.Animation.Velocity.y / 4 : fx.Animation.Velocity.y;
	int speed = TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx.RoomNumber) ? fx.Animation.Velocity.z / 4 : fx.Animation.Velocity.z;

	fx.Pose.Position.x += speed * phd_sin(fx.Pose.Orientation.y);
	fx.Pose.Position.y += fallspeed;
	fx.Pose.Position.z += speed * phd_cos(fx.Pose.Orientation.y);

	if ((fxInfo.Flag2 & BODY_DO_EXPLOSION) && !(fxInfo.Flag2 & BODY_NO_FLAME) &&
		!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx.RoomNumber))
	{
		if (GenerateInt(0, 10) > (abs(fx.Animation.Velocity.y) > 0 ? 5 : 8))
			TriggerFireFlame(fx.Pose.Position.x, fx.Pose.Position.y, fx.Pose.Position.z, FlameType::Medium);
	}

	auto pointColl = GetPointCollision(fx.Pose.Position, fx.RoomNumber);

	if (!fxInfo.Counter)
	{
		if (fx.Pose.Position.y < pointColl.GetCeilingHeight())
		{
			fx.Pose.Position.y = pointColl.GetCeilingHeight();
			fx.Animation.Velocity.y = -fx.Animation.Velocity.y;
			fx.Animation.Velocity.z -= (fx.Animation.Velocity.z / 8);
		}

		if (fx.Pose.Position.y >= pointColl.GetFloorHeight())
		{
			if (fxInfo.Flag2 & BODY_NO_BOUNCE)
			{
				fx.Pose.Position.x = x;
				fx.Pose.Position.y = y;
				fx.Pose.Position.z = z;

				if (!(fxInfo.Flag2 & BODY_NO_SHATTER_EFFECT))
				{
					if (fxInfo.Flag2 & BODY_NO_BOUNCE_ALT)
					{
						ExplodeFX(fx, -2, 32);
					}
					else
					{
						ExplodeFX(fx, -1, 32);
					}
				}
				
				// Remove if touched floor (no bounce mode).
				if (fxInfo.Flag2 & BODY_PART_EXPLODE)
					BodyPartExplode(fx);

				KillItem(fxNumber);

				if (fxInfo.Flag2 & BODY_STONE_SOUND)
					SoundEffect(SFX_TR4_ROCK_FALL_LAND, &fx.Pose);

				return;
			}

			if (y <= pointColl.GetFloorHeight())
			{
				// Remove if touched floor (no bounce mode).
				if (fxInfo.Flag2 & BODY_PART_EXPLODE)
				{
					BodyPartExplode(fx);
					KillItem(fxNumber);
				}

				if (fx.Animation.Velocity.y <= BOUNCE_FALLSPEED)
				{
					fx.Animation.Velocity.y = 0;
				}
				else
				{
					fx.Animation.Velocity.y = -fx.Animation.Velocity.y / 4;

					if (!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx.RoomNumber))
					{
						if (fxInfo.Flag2 & BODY_STONE_SOUND)
						{
							SoundEffect(SFX_TR4_ROCK_FALL_LAND, &fx.Pose);
						}
						else if (fxInfo.Flag2 & BODY_GIBS)
						{
							SoundEffect(SFX_TR4_LARA_THUD, &fx.Pose, SoundEnvironment::Land, GenerateFloat(0.8f, 1.2f));
						}
					}
				}
			}
			else
			{
				fx.Pose.Orientation.y += -ANGLE(180);
				fx.Pose.Position.x = x;
				fx.Pose.Position.z = z;
			}

			fx.Animation.Velocity.z -= (fx.Animation.Velocity.z / 4);
			if (abs(fx.Animation.Velocity.z) < 4)
				fx.Animation.Velocity.z = 0;

			fx.Pose.Position.y = y;
		}
		else
		{
			fx.Pose.Orientation.x += ANGLE(5);
			fx.Pose.Orientation.z += ANGLE(10);
		}

		if (!fx.Animation.Velocity.z && ++fxInfo.Flag1 > BODY_PART_LIFE)
		{
			if (!(fxInfo.Flag2 & BODY_NO_SMOKE))
			{
				for (int i = 0; i < 6; i++)
				{
					TriggerFlashSmoke(
						fx.Pose.Position.x + GenerateInt(-16, 16),
						fx.Pose.Position.y + GenerateInt(16, 32),
						fx.Pose.Position.z + GenerateInt(-16, 16), fx.RoomNumber);
				}
			}

			if (fxInfo.Flag2 & BODY_PART_EXPLODE)
				BodyPartExplode(fx);

			if (!(fxInfo.Flag2 & BODY_NO_SHATTER_EFFECT))
				ExplodeFX(fx, -1, 32);

			KillItem(fxNumber);
			return;
		}

		if ((fxInfo.Flag2 & BODY_GIBS) && (GetRandomControl() & 1))
		{
			for (int i = 0; i < (TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx.RoomNumber) ? 1 : 6); i++)
			{
				DoBloodSplat(
					(GetRandomControl() & 0x3F) + fx.Pose.Position.x - 32,
					(GetRandomControl() & 0x1F) + fx.Pose.Position.y - 16,
					(GetRandomControl() & 0x3F) + fx.Pose.Position.z - 32,
					1,
					2 * GetRandomControl(),
					fx.RoomNumber);
			}
		}
	}

	if (pointColl.GetRoomNumber() != fx.RoomNumber)
	{
		if (TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, pointColl.GetRoomNumber()) &&
			!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, fx.RoomNumber))
		{
			int waterHeight = GetPointCollision(fx.Pose.Position, pointColl.GetRoomNumber()).GetWaterTopHeight();

			SplashSetup.Position = Vector3(fx.Pose.Position.x, waterHeight - 1, fx.Pose.Position.z);
			SplashSetup.SplashPower = fx.Animation.Velocity.y;
			SplashSetup.InnerRadius = 48;
			SetupSplash(&SplashSetup, pointColl.GetRoomNumber());

			// Remove if touched water.
			if (fxInfo.Flag2 & BODY_PART_EXPLODE)
			{
				BodyPartExplode(fx);
				KillItem(fxNumber);
				return;
			}
		}

		ItemNewRoom(fxNumber, pointColl.GetRoomNumber());
	}
}
