#include "framework.h"
#include "Game/effects/weather.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/effects/Ripple.h"
#include "Game/effects/tomb4fx.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/game_object_ids.h"
#include "Sound/sound.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Ripple;
using namespace TEN::Math;

namespace TEN::Effects::Environment
{
	EnvironmentController Weather;

	float WeatherParticle::Transparency() const
	{
		float result = WEATHER_PARTICLE_OPACITY;
		float fade   = WEATHER_PARTICLE_NEAR_DEATH_LIFE;

		if (Life <= fade)
			result *= Life / fade;

		if ((StartLife - Life) < fade)
			result *= (StartLife - Life) / fade;

		if (Type != WeatherType::Snow)
			result *= 0.45f;

		return result;
	}

	EnvironmentController::EnvironmentController()
	{
		Particles.reserve(WEATHER_PARTICLE_COUNT_MAX);
	}

	void EnvironmentController::Update()
	{
		const auto& level = *g_GameFlow->GetLevel(CurrentLevel);

		UpdateSky(level);
		UpdateStorm(level);
		UpdateWind(level);
		UpdateFlash(level);
		UpdateWeather(level);
		UpdateStarfield(level);

		SpawnWeatherParticles(level);
		SpawnDustParticles(level);
		SpawnMeteorParticles(level);
	}

	void EnvironmentController::Clear()
	{
		// Clear storm variables.
		StormTimer     = 0;
		StormSkyColor  = 1;
		StormSkyColor2 = 1;

		// Clear wind variables.
		WindCurrent =
		WindX =
		WindZ = 0;
		WindAngle =
		WindDAngle = 2048;

		// Clear flash variables.
		FlashProgress = 0.0f;
		FlashColorBase = Vector3::Zero;

		// Clear weather.
		Particles.clear();

		// Clear starfield.
		ResetStarField = true;
		Stars.clear();
		Meteors.clear();
	}

	void EnvironmentController::Flash(int r, int g, int b, float speed)
	{
		FlashProgress = 1.0f;
		FlashSpeed = std::clamp(speed, 0.005f, 1.0f);
		FlashColorBase = Vector3(std::clamp(r, 0, UCHAR_MAX) / (float)UCHAR_MAX,
								 std::clamp(g, 0, UCHAR_MAX) / (float)UCHAR_MAX,
								 std::clamp(b, 0, UCHAR_MAX) / (float)UCHAR_MAX);
	}

	void EnvironmentController::UpdateSky(const ScriptInterfaceLevel& level)
	{
		for (int i = 0; i < 2; i++)
		{
			if (!level.GetSkyLayerEnabled(i))
				continue;

			auto& skyPos = SkyCurrentPosition[i];

			skyPos += level.GetSkyLayerSpeed(i);
			if (skyPos <= SKY_SIZE)
			{
				if (skyPos < 0)
					skyPos += SKY_SIZE;
			}
			else
			{
				skyPos -= SKY_SIZE;
			}
		}
	}

	void EnvironmentController::UpdateStorm(const ScriptInterfaceLevel& level)
	{
		if (level.GetStormEnabled())
		{
			if (StormCount || StormRand)
			{
				UpdateLightning();
				if (StormTimer > -1)
					StormTimer--;
				if (!StormTimer)
					SoundEffect(SFX_TR4_THUNDER_RUMBLE, nullptr);
			}
			else if (!(rand() & 0x7F))
			{
				StormCount = (rand() & 0x1F) + 16;
				StormTimer = (rand() & 3) + 12;
			}
		}

		for (int i = 0; i < 2; i++)
		{
			auto color = Color(
				level.GetSkyLayerColor(i).GetR() / 255.0f, 
				level.GetSkyLayerColor(i).GetG() / 255.0f,
				level.GetSkyLayerColor(i).GetB() / 255.0f,
				1.0f);

			if (level.GetStormEnabled())
			{
				auto flashBrightness = StormSkyColor / 255.0f;
				auto r = std::clamp(color.x + flashBrightness, 0.0f, 1.0f);
				auto g = std::clamp(color.y + flashBrightness, 0.0f, 1.0f);
				auto b = std::clamp(color.z + flashBrightness, 0.0f, 1.0f);

				SkyCurrentColor[i] = Vector4(r, g, b, color.w);
			}
			else
			{
				SkyCurrentColor[i] = color;
			}
		}
	}

	void EnvironmentController::UpdateLightning()
	{
		StormCount--;

		if (StormCount <= 0)
		{
			StormSkyColor = 0;
			StormRand = 0;
		}
		else if (StormCount < 5 && StormSkyColor < 50)
		{
			auto newColor = StormSkyColor - (StormCount * 2);
			if (newColor < 0)
				newColor = 0;
			StormSkyColor = newColor;
		}
		else if (StormCount)
		{
			StormRand = ((rand() & 0x1FF - StormRand) >> 1) + StormRand;
			StormSkyColor2 += StormRand * StormSkyColor2 >> 8;
			StormSkyColor = StormSkyColor2;
			if (StormSkyColor > UCHAR_MAX)
				StormSkyColor = UCHAR_MAX;
		}
	}

	void EnvironmentController::UpdateWind(const ScriptInterfaceLevel& level)
	{
		WindCurrent += (GetRandomControl() & 7) - 3;
		if (WindCurrent <= -2)
		{
			WindCurrent++;
		}
		else if (WindCurrent >= 9)
		{
			WindCurrent--;
		}

		WindDAngle = (WindDAngle + 2 * (GetRandomControl() & 63) - 64) & 0x1FFE;

		if (WindDAngle < 1024)
		{
			WindDAngle = 2048 - WindDAngle;
		}
		else if (WindDAngle > 3072)
		{
			WindDAngle += 6144 - 2 * WindDAngle;
		}

		WindAngle = (WindAngle + ((WindDAngle - WindAngle) >> 3)) & 0x1FFE;

		WindX = WindCurrent * phd_sin(WindAngle << 3);
		WindZ = WindCurrent * phd_cos(WindAngle << 3);
	}

	void EnvironmentController::UpdateFlash(const ScriptInterfaceLevel& level)
	{
		if (FlashProgress > 0.0f)
		{
			FlashProgress -= FlashSpeed;
			if (FlashProgress < 0.0f)
				FlashProgress = 0.0f;
		}

		if (FlashProgress == 0.0f)
			FlashColorBase = Vector3::Zero;
	}

	void EnvironmentController::UpdateStarfield(const ScriptInterfaceLevel& level)
	{
		int starCount = level.GetStarfieldStarCount();
		if (starCount == 0)
			return;

		if (ResetStarField)
		{
			Stars.clear();
			ResetStarField = false;
		}

		if (starCount != Stars.size())
		{
			// If starCount increased, add new stars to existing list.
			if (starCount > Stars.size())
			{
				// Reserve space for new stars if necessary.
				Stars.reserve(starCount);

				for (int i = (int)Stars.size(); i < starCount; i++)
				{
					auto starDir = Random::GenerateDirectionInCone(-Vector3::UnitY, 70.0f);
					starDir.Normalize();

					auto star = StarParticle{};
					star.Direction = starDir;
					star.Color = Vector3(
						Random::GenerateFloat(0.6f, 1.0f),
						Random::GenerateFloat(0.6f, 1.0f),
						Random::GenerateFloat(0.6f, 1.0f));
					star.Scale = Random::GenerateFloat(0.5f, 1.5f);

					float cosine = Vector3::UnitY.Dot(starDir);
					float maxCosine = cos(DEG_TO_RAD(50.0f));
					float minCosine = cos(DEG_TO_RAD(70.0f));

					if (cosine >= minCosine && cosine <= maxCosine)
					{
						star.Extinction = (cosine - minCosine) / (maxCosine - minCosine);
					}
					else
					{
						star.Extinction = 1.0f;
					}

					Stars.push_back(star);
				}
			}
			// If starCount decreased, resize vector without reinitializing.
			else
			{
				Stars.resize(starCount);
			}

		}

		for (auto& star : Stars)
			star.Blinking = Random::GenerateFloat(0.5f, 1.0f);

		if (level.GetStarfieldMeteorCount() > 0)
		{
			for (auto& meteor : Meteors)
			{
				meteor.Life--;

				if (meteor.Life <= 0)
				{
					meteor.Active = false;
					continue;
				}

				meteor.StoreInterpolationData();

				if (meteor.Life <= METEOR_PARTICLE_FADE_TIME)
				{
					meteor.Fade = meteor.Life / METEOR_PARTICLE_FADE_TIME;
				}
				else if (meteor.Life >= METEOR_PARTICLE_LIFE_MAX - METEOR_PARTICLE_FADE_TIME)
				{
					meteor.Fade = (METEOR_PARTICLE_LIFE_MAX - meteor.Life) / METEOR_PARTICLE_FADE_TIME;
				}
				else
				{
					meteor.Fade = 1.0f;
				}

				meteor.Position += meteor.Direction * level.GetStarfieldMeteorVelocity();
			}
		}		
	}

	void EnvironmentController::UpdateWeather(const ScriptInterfaceLevel& level)
	{
		for (auto& part : Particles)
		{
			part.StoreInterpolationData();

			part.Life -= 2;

			// Disable particle if dead. Will be cleaned on next call of SpawnWeatherParticles().
			if (part.Life <= 0)
			{
				part.Enabled = false;
				continue;
			}

			// Check if particle got out of collision check radius and fade out if it did.
			if (abs(Camera.pos.x - part.Position.x) > COLLISION_CHECK_DISTANCE ||
				abs(Camera.pos.z - part.Position.z) > COLLISION_CHECK_DISTANCE)
			{
				part.Life = std::clamp(part.Life, 0.0f, WEATHER_PARTICLE_NEAR_DEATH_LIFE);
			}

			// If particle was locked (after landing or stucking in substance such as water or swamp),
			// fade out and bypass collision checks and movement updates.
			if (part.Stopped)
			{
				if (part.Type == WeatherType::Snow)
					part.Size *= WEATHER_PARTICLE_NEAR_DEATH_MELT_FACTOR;

				continue;
			}

			// Backup previous position and progress new position according to velocity.
			auto prevPos = part.Position;
			part.Position.x += part.Velocity.x;
			part.Position.z += part.Velocity.z;

			switch (part.Type)
			{
			case WeatherType::None:
				part.Position.y += part.Velocity.y;
				break;

			case WeatherType::Rain:
			case WeatherType::Snow:
				part.Position.y += (part.Velocity.y / 2.0f);
				break;
			}

			// Particle is inert; don't check collisions.
			if (part.Type == WeatherType::None)
				continue;

			PointCollisionData pointColl;
			bool collisionCalculated = false;
			bool landed = false;

			if (part.CollisionCheckDelay <= 0)
			{
				pointColl = GetPointCollision(part.Position, part.RoomNumber);

				// Determine collision checking frequency based on nearest floor/ceiling surface position.
				// If floor and ceiling are too far, don't do precise collision checks, instead doing it 
				// every 5th frame. If particle approaches floor or ceiling, make checks more frequent.
				// This avoids calls to GetPointCollision() for every particle.
				
				auto coeff = std::min(std::max(0.0f, (pointColl.GetFloorHeight() - part.Position.y)), std::max(0.0f, (part.Position.y - pointColl.GetCeilingHeight())));
				part.CollisionCheckDelay = std::min(floor(coeff / std::max(std::numeric_limits<float>::denorm_min(), part.Velocity.y)), WEATHER_PARTICLE_COLL_CHECK_DELAY_MAX);
				collisionCalculated = true;
			}
			else
			{
				part.CollisionCheckDelay--;
			}

			// Check if particle is beyond room bounds.
			if (!IsPointInRoom(part.Position, part.RoomNumber))
			{
				if (!collisionCalculated)
				{
					pointColl = GetPointCollision(part.Position, part.RoomNumber);
					collisionCalculated = true;
				}

				if (pointColl.GetRoomNumber() == part.RoomNumber)
				{
					// Not landed on door, so out of room bounds - land.
					landed = true;
				}
				else
				{
					part.RoomNumber = pointColl.GetRoomNumber();
				}
			}

			// If collision was updated, process with position checks.
			if (collisionCalculated)
			{
				// If particle is inside water or swamp, count it as "inSubstance".
				// If particle got below floor or above ceiling, count it as "landed".
				bool inSubstance = g_Level.Rooms[pointColl.GetRoomNumber()].flags & (ENV_FLAG_WATER | ENV_FLAG_SWAMP);
				landed = landed || (pointColl.GetFloorHeight() <= part.Position.y) || (pointColl.GetCeilingHeight() >= part.Position.y);

				if (inSubstance || landed)
				{
					part.Stopped = true;
					part.Position = prevPos;
					part.Life = std::clamp(part.Life, 0.0f, WEATHER_PARTICLE_NEAR_DEATH_LIFE);

					// Produce ripples if particle got into substance (water or swamp).
					if (inSubstance)
					{
						auto ripplePos = part.Position;
						ripplePos.y = pointColl.GetWaterSurfaceHeight();
						SpawnRipple(ripplePos, part.RoomNumber, Random::GenerateFloat(16.0f, 24.0f), (int)RippleFlags::SlowFade | (int)RippleFlags::LowOpacity);
					}

					// Immediately disable rain particle because it doesn't need fading out.
					if (part.Type == WeatherType::Rain)
					{
						part.Enabled = false;
						AddWaterSparks(prevPos.x, inSubstance ? pointColl.GetWaterSurfaceHeight() : pointColl.GetFloorHeight() - 32, prevPos.z, 6);
					}

					continue;
				}
			}

			// Update velocities for every particle type.
			switch (part.Type)
			{
			case WeatherType::Snow:

				if (part.Velocity.x < (WindX << 2))
				{
					part.Velocity.x += Random::GenerateFloat(0.5f, 2.5f);
				}
				else if (part.Velocity.x > (WindX << 2))
				{
					part.Velocity.x -= Random::GenerateFloat(0.5f, 2.5f);
				}

				if (part.Velocity.z < (WindZ << 2))
				{
					part.Velocity.z += Random::GenerateFloat(0.5f, 2.5f);
				}
				else if (part.Velocity.z > (WindZ << 2))
				{
					part.Velocity.z -= Random::GenerateFloat(0.5f, 2.5f);
				}

				if (part.Velocity.y < part.Size / 2)
					part.Velocity.y += part.Size / 5.0f;

				break;

			case WeatherType::Rain:

				auto random = Random::GenerateInt();
				if ((random & 3) != 3)
				{
					part.Velocity.x += (float)((random & 3) - 1);
					if (part.Velocity.x < -4)
					{
						part.Velocity.x = -4;
					}
					else if (part.Velocity.x > 4)
					{
						part.Velocity.x = 4;
					}
				}

				random = (random >> 2) & 3;
				if (random != 3)
				{
					part.Velocity.z += random - 1;
					if (part.Velocity.z < -4)
					{
						part.Velocity.z = -4;
					}
					else if (part.Velocity.z > 4)
					{
						part.Velocity.z = 4;
					}
				}

				if (part.Velocity.y < part.Size * 2 * std::clamp(level.GetWeatherStrength(), 0.6f, 1.0f))
					part.Velocity.y += part.Size / 5.0f;

				break;
			}
		}
	}

	void EnvironmentController::SpawnDustParticles(const ScriptInterfaceLevel& level)
	{
		for (int i = 0; i < DUST_SPAWN_DENSITY; i++)
		{
			// TODO: Use functions in Math::Random namespace.
			auto pos = Camera.pos.ToVector3i() + Vector3i(
				(rand() % DUST_SPAWN_RADIUS) - (DUST_SPAWN_RADIUS / 2),
				(rand() % DUST_SPAWN_RADIUS) - (DUST_SPAWN_RADIUS / 2),
				(rand() % DUST_SPAWN_RADIUS) - (DUST_SPAWN_RADIUS / 2));

			int roomNumber = Camera.pos.RoomNumber;
			if (!IsPointInRoom(pos, roomNumber))
				roomNumber = FindRoomNumber(pos, Camera.pos.RoomNumber, true);

			if (!IsPointInRoom(pos, roomNumber) || roomNumber == NO_VALUE)
				continue;

			// Check if water room.
			if (!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, roomNumber))
				continue;

			auto part = WeatherParticle();
			part.Velocity = Random::GenerateDirection() * DUST_VELOCITY_MAX;
			part.Size = Random::GenerateFloat(DUST_SIZE_MAX / 2, DUST_SIZE_MAX);
			part.Type = WeatherType::None;
			part.Life = DUST_LIFE + Random::GenerateInt(-10, 10);
			part.RoomNumber = roomNumber;
			part.Position = pos.ToVector3();
			part.Stopped = false;
			part.Enabled = true;
			part.StartLife = part.Life;
			Particles.push_back(part);
		}
	}

	void EnvironmentController::SpawnWeatherParticles(const ScriptInterfaceLevel& level)
	{
		// Clean up dead particles.
		if (Particles.size() > 0)
		{
			Particles.erase(
				std::remove_if(
					Particles.begin(), Particles.end(),
					[](const WeatherParticle& part)
					{
						return !part.Enabled;
					}),
				Particles.end());
		}

		if (level.GetWeatherType() == WeatherType::None || level.GetWeatherStrength() == 0.0f)
			return;

		int newParticlesCount = 0;
		int density = WEATHER_PARTICLE_SPAWN_DENSITY * level.GetWeatherStrength();

		// Snow is falling twice as fast and must be spawned accordingly fast.
		if (level.GetWeatherType() == WeatherType::Snow)
			density *= 2;

		if (density > 0.0f && level.GetWeatherType() != WeatherType::None)
		{
			while (Particles.size() < WEATHER_PARTICLE_COUNT_MAX)
			{
				if (newParticlesCount > density)
					break;

				newParticlesCount++;

				float dist = (level.GetWeatherType() == WeatherType::Snow) ? COLLISION_CHECK_DISTANCE : (COLLISION_CHECK_DISTANCE / 2);
				float radius = Random::GenerateInt(0, dist);
				short angle = Random::GenerateInt(ANGLE(0), ANGLE(180));

				auto xPos = Camera.pos.x + ((int)(phd_cos(angle) * radius));
				auto zPos = Camera.pos.z + ((int)(phd_sin(angle) * radius));
				auto yPos = Camera.pos.y - (BLOCK(3) + Random::GenerateInt() & (BLOCK(4) - 1));
				
				auto outsideRoom = IsRoomOutside(xPos, yPos, zPos);
				
				if (outsideRoom == NO_VALUE)
					continue;

				if (g_Level.Rooms[outsideRoom].flags & (ENV_FLAG_WATER | ENV_FLAG_SWAMP))
					continue;

				auto pointColl = GetPointCollision(Vector3i(xPos, yPos, zPos), outsideRoom);

				if (!(pointColl.GetCeilingHeight() < yPos || pointColl.GetSector().GetNextRoomNumber(Vector3i(xPos, yPos, zPos), false).has_value()))
					continue;

				auto part = WeatherParticle();

				switch (level.GetWeatherType())
				{
				case WeatherType::Snow:
					part.ClusterSize = (int)(level.GetWeatherStrength() * WEATHER_PARTICLE_CLUSTER_MULT / 2);
					part.Size = Random::GenerateFloat(SNOW_SIZE_MAX / 3, SNOW_SIZE_MAX);
					part.Velocity.y = Random::GenerateFloat(SNOW_VELOCITY_MAX / 4, SNOW_VELOCITY_MAX) * (part.Size / SNOW_SIZE_MAX);
					part.Life = (SNOW_VELOCITY_MAX / 3) + ((SNOW_VELOCITY_MAX / 2) - ((int)part.Velocity.y >> 2));
					break;

				case WeatherType::Rain:
					part.ClusterSize = (int)(level.GetWeatherStrength() * WEATHER_PARTICLE_CLUSTER_MULT);
					part.Size = Random::GenerateFloat(RAIN_SIZE_MAX / 2, RAIN_SIZE_MAX);
					part.Velocity.y = Random::GenerateFloat(RAIN_VELOCITY_MAX / 2, RAIN_VELOCITY_MAX) * (part.Size / RAIN_SIZE_MAX) * std::clamp(level.GetWeatherStrength(), 0.6f, 1.0f);
					part.Life = (RAIN_VELOCITY_MAX * 2) - part.Velocity.y;
					break;
				}

				part.Velocity.x = Random::GenerateFloat(WEATHER_PARTICLE_HORIZONTAL_VELOCITY / 2, WEATHER_PARTICLE_HORIZONTAL_VELOCITY);
				part.Velocity.z = Random::GenerateFloat(WEATHER_PARTICLE_HORIZONTAL_VELOCITY / 2, WEATHER_PARTICLE_HORIZONTAL_VELOCITY);

				part.UniqueID = (int)Particles.size();
				part.Type = level.GetWeatherType();
				part.RoomNumber = outsideRoom;
				part.Position.x = xPos;
				part.Position.y = yPos;
				part.Position.z = zPos;
				part.Stopped = false;
				part.Enabled = true;
				part.CollisionCheckDelay = 0;
				part.StartLife = part.Life;

				Particles.push_back(part);
			}
		}
	}

	void EnvironmentController::SpawnMeteorParticles(const ScriptInterfaceLevel& level)
	{
		// Clean up dead particles.
		if (!Meteors.empty())
		{
			Meteors.erase(
				std::remove_if(
					Meteors.begin(), Meteors.end(),
					[](const MeteorParticle& part)
					{
						return !part.Active;
					}),
				Meteors.end());
		}

		if (level.GetStarfieldMeteorCount() == 0)
			return;

		int density = level.GetStarfieldMeteorSpawnDensity();
		if (density > 0)
		{
			int newParticlesCount = 0;

			while (Meteors.size() < level.GetStarfieldMeteorCount())
			{
				if (newParticlesCount > density)
					break;

				auto horizontalDir = Random::GenerateDirection2D();

				auto part = MeteorParticle();

				part.Active = true;
				part.Life = METEOR_PARTICLE_LIFE_MAX;
				part.StartPosition =
					part.Position = Random::GenerateDirectionInCone(-Vector3::UnitY, 40.0f) * BLOCK(1.5f);
				part.Fade = 0.0f;
				part.Color = Vector3(
					Random::GenerateFloat(0.6f, 1.0f),
					Random::GenerateFloat(0.6f, 1.0f),
					Random::GenerateFloat(0.6f, 1.0f));

				part.Direction = Random::GenerateDirectionInCone(Vector3(horizontalDir.x, 0, horizontalDir.y), 10.0f);
				if (part.Direction.y < 0.0f)
					part.Direction.y = -part.Direction.y;

				Meteors.push_back(part);

				newParticlesCount++;
			}
		}
	}
}
