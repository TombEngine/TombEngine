#include "framework.h"
#include "Objects/TR5/Emitter/tr5_smoke_emitter.h"

#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/control/trigger.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Math/Math.h"
#include "Objects/objectslist.h"
#include "Renderer/RendererEnums.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Effects::Bubble;
using namespace TEN::Math;

// NOTES:
// OCB 0:	 Default smoke behaviour.
// OCB < 0:	 Disable harm from the steam by setting the NoDamage flag in ItemFlags[3].
// OCB != 0: Enable horizontal steam effect.
// OCB >> 4: Sets number of game frames it pauses between steam releases. Calculation: x = 16 * gameFrameCount.
// 
// item.ItemFlags[0]: Timer in frame time for pause between steam releases
// item.ItemFlags[1]: Timer in frame time for active steam effect.
// item.ItemFlags[2]: Acceleration of steam particles.
// item.ItemFlags[3]: Smoke emiter flags.
// 
// In underwater rooms:
// OCB 0: Intermitent bubble emission.
// OCB 1: Continuous bubble emission.
// OCB +2: Spawn large bubbles.
// 
// item.ItemFlags[0]: Bubble count.
// item.ItemFlags[1]: Spawn a series of bubbles with no delay (flag).
// item.ItemFlags[2]: Bubble spawn radius on horizontal plane (default is 32).

namespace TEN::Effects::SmokeEmitter
{
	constexpr auto SMOKE_VISIBILITY_DISTANCE_MAX = BLOCK(16);
	constexpr auto SMOKE_ACCEL_MAX				 = BLOCK(4);
	constexpr auto BUBBLE_DEFAULT_RADIUS		 = BLOCK(1 / 32.0f);

	enum SmokeEmitterFlags
	{
		NoDamage = 1 << 0
	};

	static void SpawnSteamParticle(const ItemInfo& item, int currentAccel)
	{
		constexpr auto COLOR_BLACK			= Color(0.4f, 0.4f, 0.4f);
		constexpr auto COLOR_WHITE_START	= Color(0.4f, 0.4f, 0.4f);
		constexpr auto COLOR_WHITE_END		= Color(0.25f, 0.25f, 0.25f);
		constexpr auto LIFE_MAX				= 24;
		constexpr auto LIFE_MIN				= 16;
		constexpr auto PART_SIZE_MAX		= 160.0f;
		constexpr auto PART_SIZE_MIN		= 128.0f;
		constexpr auto FADE_SPEED_MAX		= 9;
		constexpr auto FADE_SPEED_MIN		= 6;
		constexpr auto DAMAGE_TIME_INTERVAL = 1;

		static const auto SPHERE = BoundingSphere(Vector3::Zero, 32.0f);

		auto& part = *GetFreeParticle();
		part.on = true;

		if (item.ObjectNumber == ID_SMOKE_EMITTER_BLACK)
		{
			part.sR = COLOR_BLACK.R() * UCHAR_MAX;
			part.sG = COLOR_BLACK.G() * UCHAR_MAX;
			part.sB = COLOR_BLACK.B() * UCHAR_MAX;
			part.dR = COLOR_BLACK.R() * UCHAR_MAX;
			part.dG = COLOR_BLACK.G() * UCHAR_MAX;
			part.dB = COLOR_BLACK.B() * UCHAR_MAX;
		}
		else if (item.ObjectNumber == ID_SMOKE_EMITTER_WHITE)
		{
			part.sR = COLOR_WHITE_START.R() * UCHAR_MAX;
			part.sG = COLOR_WHITE_START.G() * UCHAR_MAX;
			part.sB = COLOR_WHITE_START.B() * UCHAR_MAX;
			part.dR = COLOR_WHITE_END.R() * UCHAR_MAX;
			part.dG = COLOR_WHITE_END.G() * UCHAR_MAX;
			part.dB = COLOR_WHITE_END.B() * UCHAR_MAX;
		}
		else
		{
			unsigned char r = std::clamp(item.Model.Color.x / 2, 0.0f, 1.0f) * UCHAR_MAX;
			unsigned char g = std::clamp(item.Model.Color.y / 2, 0.0f, 1.0f) * UCHAR_MAX;
			unsigned char b = std::clamp(item.Model.Color.z / 2, 0.0f, 1.0f) * UCHAR_MAX;

			part.sR = r / 3;
			part.sG = g / 3;
			part.sB = b / 3;
			part.dR = r;
			part.dG = g;
			part.dB = b;
		}
		
		part.fadeToBlack = 6;
		part.colFadeSpeed = Random::GenerateInt(FADE_SPEED_MIN, FADE_SPEED_MAX);

		if (item.ObjectNumber == ID_SMOKE_EMITTER_BLACK)
		{
			part.blendMode = BlendMode::Subtractive;
		}
		else
		{
			part.blendMode = BlendMode::Additive;
		}

		part.life =
		part.sLife = Random::GenerateInt(LIFE_MIN, LIFE_MAX);

		auto pos = item.Pose.Position.ToVector3() + Random::GeneratePointInSphere(SPHERE);
		part.x = pos.x;
		part.y = pos.y;
		part.z = pos.z;

		int accel = currentAccel;
		if (currentAccel == SMOKE_ACCEL_MAX)
			accel = Random::GenerateInt(SMOKE_ACCEL_MAX / 2, SMOKE_ACCEL_MAX - 1);

		int pitchAngle = item.Pose.Orientation.x;
		int yawAngle = item.Pose.Orientation.y + ANGLE(180.0f);

		auto dir = Vector3::Zero;;
		dir.x = phd_cos(pitchAngle) * phd_sin(yawAngle);
		dir.y = phd_sin(pitchAngle);
		dir.z = phd_cos(pitchAngle) * phd_cos(yawAngle);

		dir.Normalize();
		int offset = Random::GenerateInt(-8, 8);
		part.xVel = (dir.x * accel) + offset;
		part.yVel = (dir.y * accel) + offset;
		part.zVel = (dir.z * accel) + offset;

		part.friction = 4;
		part.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;

		bool ignoreDamage = item.ItemFlags[3] & SmokeEmitterFlags::NoDamage;
		if (!ignoreDamage && TestGlobalTimeInterval(DAMAGE_TIME_INTERVAL))
		{
			part.flags |= SP_DAMAGE;
			part.damage = 2;
		}
			
		part.rotAng = Random::GenerateAngle(ANGLE(0.0f), ANGLE(22.5f));
		part.rotAdd = Random::GenerateAngle(ANGLE(0.04f), ANGLE(0.08f)) * (Random::TestProbability(1 / 2.0f) ? 1 : -1);

		part.scalar = 2;
		part.gravity = Random::GenerateFloat(-24.0f, -15.0f);
		part.maxYvel = 0;
		
		float size = Random::GenerateFloat(PART_SIZE_MIN, PART_SIZE_MAX);
		part.dSize = size;
		part.sSize =
		part.size = part.dSize / 2;
	}

	static void SpawnSmokeEmitterParticle(const ItemInfo& item)
	{
		auto& part = *GetFreeParticle();
		part.on = true;

		part.sR = 0;
		part.sG = 0;
		part.sB = 0;

		if (item.ObjectNumber == ID_SMOKE_EMITTER_BLACK)
		{
			part.dR = 0.4f * UCHAR_MAX;
			part.dG = 0.4f * UCHAR_MAX;
			part.dB = 0.4f * UCHAR_MAX;
		}
		else if (item.ObjectNumber == ID_SMOKE_EMITTER_WHITE)
		{
			part.dR = 0.25f * UCHAR_MAX;
			part.dG = 0.25f * UCHAR_MAX;
			part.dB = 0.25f * UCHAR_MAX;
		}
		else
		{
			unsigned char r = std::clamp(item.Model.Color.x / 2.0f, 0.0f, 1.0f) * UCHAR_MAX;
			unsigned char g = std::clamp(item.Model.Color.y / 2.0f, 0.0f, 1.0f) * UCHAR_MAX;
			unsigned char b = std::clamp(item.Model.Color.z / 2.0f, 0.0f, 1.0f) * UCHAR_MAX;

			part.dR = r;
			part.dG = g;
			part.dB = b;
		}

		part.fadeToBlack = 16;
		part.colFadeSpeed = Random::GenerateInt(8, 11);

		if (item.ObjectNumber == ID_SMOKE_EMITTER_BLACK)
		{
			part.blendMode = BlendMode::Subtractive;
		}
		else
		{
			part.blendMode = BlendMode::Additive;
		}

		part.sLife = part.life = Random::GenerateInt(28, 35);

		part.x = item.Pose.Position.x + Random::GenerateInt(-32, 32);
		part.y = item.Pose.Position.y + Random::GenerateInt(-32, 32);
		part.z = item.Pose.Position.z + Random::GenerateInt(-32, 32);

		part.xVel = Random::GenerateInt(-128, 128);
		part.yVel = Random::GenerateInt(-16, 0);
		part.zVel = Random::GenerateInt(-128, 128);

		part.friction = 3;
		part.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;

		if (TestEnvironment(RoomEnvFlags::ENV_FLAG_SKYBOX, item.RoomNumber))
			part.flags |= SP_WIND;

		part.rotAng = Random::GenerateInt(0, 4095);

		if (Random::TestProbability(1 / 2.0f))
		{
			part.rotAdd = Random::GenerateInt(-15, -7);
		}
		else
		{
			part.rotAdd = Random::GenerateInt(7, 15);
		}

		part.scalar = 2;
		part.gravity = Random::GenerateInt(-24, -15);
		part.maxYvel = Random::GenerateInt(-15, -7);

		float size = Random::GenerateFloat(128.0f, 160.0f);
		part.dSize = size;
		part.sSize =
		part.size = size / 4;

		if (item.ObjectNumber == ID_SMOKE_EMITTER)
		{
			part.gravity /= 2;
			part.yVel /= 2;
			part.maxYvel /= 2;
			part.life += 16;
			part.sLife += 16;
		}
	}

	static void SpawnSmokeEmitterBubble(const ItemInfo& item, int radius, bool spawnLargeBubbles)
	{
		auto pos = item.Pose.Position.ToVector3() + Vector3(Random::GenerateInt(-radius, radius), Random::GenerateInt(-16, 16), Random::GenerateInt(-radius, radius));

		if (spawnLargeBubbles)
		{
			SpawnBubble(pos, item.RoomNumber, (int)BubbleFlags::HighAmplitude | (int)BubbleFlags::LargeScale);
		}
		else
		{
			SpawnBubble(pos, item.RoomNumber);
		}
	}

	void InitializeSmokeEmitter(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		
		bool isUnderwater = TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, item.RoomNumber);
		bool isSteamEffect = (item.TriggerFlags != 0);

		if (isUnderwater)
		{
			auto& bubbleCount = item.ItemFlags[0];
			auto& bubbleForceSpawnFlag = item.ItemFlags[1];
			auto& bubbleSpawnRadius = item.ItemFlags[2];

			bool spawnContinuousBubbles = (item.TriggerFlags == 1);
			if (spawnContinuousBubbles)
			{
				bubbleCount = 20;
				bubbleForceSpawnFlag = 1;
			}

			if (bubbleSpawnRadius == 0)
				bubbleSpawnRadius = BUBBLE_DEFAULT_RADIUS;
		}
		else if (isSteamEffect)
		{
			auto& ocb = item.TriggerFlags;
			auto& steamPauseTimer = item.ItemFlags[0];
			auto& steamAccel = item.ItemFlags[2];
			auto& steamFlags = item.ItemFlags[3];

			if (ocb < 0)
			{
				ocb = -ocb;
				steamFlags |= SmokeEmitterFlags::NoDamage;
			}

			steamPauseTimer = ocb / 16;

			if (steamPauseTimer <= 0)
				steamAccel = SMOKE_ACCEL_MAX;
		}
	}

	void ControlSmokeEmitter(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		// Draw underwater bubbles.
		if (TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, item.RoomNumber))
		{
			bool spawnContinuousBubbles = (item.TriggerFlags == 1);
			bool spawnLargeBubbles = item.TriggerFlags & 2;

			auto& bubbleCount = item.ItemFlags[0];
			auto& bubbleForceSpawnFlag = item.ItemFlags[1];
			auto& bubbleSpawnRadius = item.ItemFlags[2];

			if (bubbleCount || Random::TestProbability(1 / 30.0f) || spawnContinuousBubbles)
			{
				if (Random::TestProbability(1 / 3.0f) || bubbleForceSpawnFlag)
				{
					SpawnSmokeEmitterBubble(item, bubbleSpawnRadius, spawnLargeBubbles);

					if (bubbleCount)
					{
						bubbleCount--;

						if (!bubbleCount)
							bubbleForceSpawnFlag = 0;
					}
				}
			}
			else if (Random::TestProbability(1 / 30.0f))
			{
				bubbleCount = Random::GenerateInt(4, 7);
			}

			return;
		}

		// Draw horizontal steam.
		bool isSteamShotEffect = item.TriggerFlags != 0;
		if (isSteamShotEffect)
		{
			bool drawNormalSmoke = false;

			auto& steamPauseTimer = item.ItemFlags[0];
			auto& steamActiveTimer = item.ItemFlags[1];
			auto& steamAccel = item.ItemFlags[2];

			if (steamPauseTimer != 0)
			{
				drawNormalSmoke = true;
				steamPauseTimer--;

				if (steamPauseTimer <= 0)
				{
					steamActiveTimer = Random::GenerateInt(30, 94);
					SoundEffect(SFX_TR4_STEAM, &item.Pose);
				}
								
				if (steamAccel)
					steamAccel -= 256;
			}
			else if (steamAccel < SMOKE_ACCEL_MAX)
			{
				steamAccel += 256;
			}

			if (steamAccel != 0)
			{
				SpawnSteamParticle(item, steamAccel);

				if (steamActiveTimer)
				{
					steamActiveTimer--;
				}
				else
				{
					steamPauseTimer = item.TriggerFlags >> 4;
				}

				SoundEffect(SFX_TR4_STEAM_EMITTER_LOOP, &item.Pose);
			}

			if (!drawNormalSmoke)
				return;
		}

		// Draw normal smoke.
		bool drawSmoke = (!(Wibble & 0x0F) && (item.ObjectNumber != ID_SMOKE_EMITTER || !(Wibble & 0x1F)));
		if (drawSmoke)
			SpawnSmokeEmitterParticle(item);
	}
}
