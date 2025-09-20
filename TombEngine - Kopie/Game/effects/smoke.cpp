#include "framework.h"
#include "Game/effects/smoke.h"

#include <algorithm>
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"

using namespace TEN::Effects::Environment;
using namespace TEN::Math;

namespace TEN::Effects::Smoke
{
	std::array<SmokeParticle, 128> SmokeParticles;

	auto& GetFreeSmokeParticle()
	{
		for (int i = 0; i < SmokeParticles.size(); i++)
		{
			if (!SmokeParticles[i].active)
				return SmokeParticles[i];
		}

		return SmokeParticles[0];
	}

	void DisableSmokeParticles()
	{
		for (int i = 0; i < SmokeParticles.size(); i++)
			SmokeParticles[i].active = false;
	}

	void UpdateSmokeParticles()
	{
		for (int i = 0; i < SmokeParticles.size(); i++)
		{
			auto& s = SmokeParticles[i];

			if (!s.active)
				continue;

			s.StoreInterpolationData();

			s.age += 1;
			if (s.age > s.life)
			{
				s.active = false;
				continue;
			}

			s.velocity.y += s.gravity;

			if (s.terminalVelocity != 0)
			{
				float velocityLength = s.velocity.Length();

				if (velocityLength > s.terminalVelocity)
					s.velocity *= (s.terminalVelocity / velocityLength);
			}

			s.position += s.velocity;

			if (s.affectedByWind)
			{
				if (TestEnvironment(ENV_FLAG_WIND, s.room))
				{
					s.position.x += Weather.Wind().x;
					s.position.z += Weather.Wind().z;
				}
			}

			float normalizedLife = std::clamp(s.age / s.life,0.0f,1.0f);
			s.size = Lerp(s.sourceSize, s.destinationSize, normalizedLife);
			s.angularVelocity *= s.angularDrag;
			s.rotation += s.angularVelocity;
			s.color = DirectX::SimpleMath::Vector4::Lerp(s.sourceColor, s.destinationColor, normalizedLife);
			int numSprites = -Objects[ID_SMOKE_SPRITES].nmeshes;
			s.sprite = Lerp(0, numSprites - 1, normalizedLife);
		}
	}

	void TriggerFlareSmoke(const Vector3& pos, const Vector3& direction, int life, int room)
	{
		auto& s = GetFreeSmokeParticle();
		s = {};
		s.position = pos;
		s.age = 0;
		constexpr float d = 0.2f;

		auto randomDir = Vector3(Random::GenerateFloat(-d, d), Random::GenerateFloat(-d, d), Random::GenerateFloat(-d, d));
		auto direction2 = Vector3::Zero;
		(direction + randomDir).Normalize(direction2);

		s.velocity = direction2 * Random::GenerateFloat(7, 9);
		s.gravity = -0.2f;
		s.friction = Random::GenerateFloat(0.7f, 0.85f);
		s.sourceColor = g_GameFlow->GetSettings()->Flare.Color * Vector4(0.5f);
		s.destinationColor = Vector4(1, 1, 1, 0);
		s.life = Random::GenerateFloat(25, 35);
		s.angularVelocity = Random::GenerateFloat(-0.3f, 0.3f);
		s.angularDrag = 0.97f;
		s.sourceSize = life > 4 ? Random::GenerateFloat(16, 24) : Random::GenerateFloat(100, 128);
		s.destinationSize = life > 4 ? Random::GenerateFloat(160, 200) : Random::GenerateFloat(256, 300);
		s.affectedByWind = true;
		s.active = true;
		s.room = room;
	}

	//TODO: add additional "Weapon Special" param or something. Currently initial == 2 means Rocket Launcher backwards smoke.
	//TODO: Refactor different weapon types out of it
	void TriggerGunSmokeParticles(int x, int y, int z, int xv, int yv, int zv, byte initial, LaraWeaponType weaponType, byte count)
	{
		SpawnGunSmokeParticles(Vector3(x, y, z), Vector3(xv, yv, zv), LaraItem->RoomNumber, initial, weaponType, count);
	}

	void SpawnGunSmokeParticles(const Vector3& pos, const Vector3& direction, int roomNumber, byte initial, LaraWeaponType weaponType, int count)
	{
		if (!g_GameFlow->GetSettings()->Weapons[(int)weaponType - 1].Smoke)
			return;

		auto& s = GetFreeSmokeParticle();
		s = {};
		s.active = true;
		s.position = pos;

		auto directionNorm = direction;
		directionNorm.Normalize();

		s.velocity = directionNorm;
		s.gravity = -.1f;
		s.affectedByWind = TestEnvironment(ENV_FLAG_WIND, pos.x, pos.y, pos.z, roomNumber);
		s.sourceColor = Vector4(0.4f, 0.4f, 0.4f, 1);
		s.destinationColor = Vector4::Zero;

		if (initial)
		{
			if (weaponType == LaraWeaponType::RocketLauncher)
			{
				float size = Random::GenerateFloat(48, 80);
				s.sourceSize = size * 2;
				s.destinationSize = size * 8;
				s.sourceColor = { 0.75, 0.75, 1, 1 };
				s.terminalVelocity = 0;
				s.friction = 0.82f;
				s.life = Random::GenerateFloat(60, 90);

				if (initial == 1)
				{
					float size = Random::GenerateFloat(48, 80);
					s.sourceSize = size * 2;
					s.destinationSize = size * 16;
					s.velocity = Random::GenerateDirectionInCone(directionNorm, 25);
					s.velocity *= Random::GenerateFloat(0, 32);
				}
				else
				{
					float size = Random::GenerateFloat(48, 80);
					s.sourceSize = size;
					s.destinationSize = size * 8;
					s.velocity = Random::GenerateDirectionInCone(directionNorm, 3);
					s.velocity *= Random::GenerateFloat(0, 16);
				}
			}
			else
			{
				float size = Random::GenerateFloat(48, 73);
				s.sourceSize = size * 2;
				s.destinationSize = size * 8;
				s.terminalVelocity = 0;
				s.friction = 0.88f;
				s.life = Random::GenerateFloat(60, 90);
				s.velocity = Random::GenerateDirectionInCone(directionNorm, 10);
				s.velocity *= Random::GenerateFloat(16, 30);
			}
		}
		else
		{
			float size = (float)((GetRandomControl() & 0x0F) + 48);

			if (weaponType == LaraWeaponType::RocketLauncher)
				s.sourceColor = { 0.75, 0.75, 1, 1 };

			s.sourceSize = size / 2;
			s.destinationSize = size * 4;
			s.terminalVelocity = 0;
			s.friction = 0.97f;
			s.life = Random::GenerateFloat(42, 62);
			s.velocity *= Random::GenerateFloat(16, 40);
		}

		s.position = pos;
		s.position += Vector3(Random::GenerateFloat(-8, 8), Random::GenerateFloat(-8, 8), Random::GenerateFloat(-8, 8));
		s.angularVelocity = Random::GenerateFloat(-PI_DIV_4, PI_DIV_4);
		s.angularDrag = 0.95f;
		s.room = roomNumber;
	}

	void TriggerQuadExhaustSmoke(int x, int y, int z, short angle, int velocity, int moving)
	{
		auto& s = GetFreeSmokeParticle();
		s = {};
		s.position = Vector3(x, y, z) + Vector3(Random::GenerateFloat(8, 16), Random::GenerateFloat(8, 16), Random::GenerateFloat(8, 16));

		float xVel = std::sin(TO_RAD(angle)) * velocity;
		float zVel = std::cos(TO_RAD(angle)) * velocity;

		s.velocity = Vector3(xVel, Random::GenerateFloat(-1, 4), zVel);
		s.sourceColor = Vector4(1, 1, 1, 1);
		s.destinationColor = Vector4::Zero;
		s.sourceSize = Random::GenerateFloat(8,24);
		s.active = true;
		s.affectedByWind = true;
		s.friction = 0.999f;
		s.gravity = -0.1f;
		s.life = Random::GenerateFloat(16, 24);
		s.destinationSize = Random::GenerateFloat(128, 160);
		s.angularVelocity = Random::GenerateFloat(-1, 1);
		s.angularDrag = Random::GenerateFloat(0.97f, 0.999f);
	}

	void TriggerRocketSmoke(int x, int y, int z)
	{
		auto& s = GetFreeSmokeParticle();
		s = {};
		s.position = Vector3(x, y, z) + Vector3(Random::GenerateFloat(8.0f, 16.0f), Random::GenerateFloat(8.0f, 16.0f), Random::GenerateFloat(8.0f, 16.0f));
		s.sourceColor = Vector4(0.8f, 0.8f, 1, 1);
		s.destinationColor = Vector4::Zero;
		s.sourceSize = Random::GenerateFloat(32.0f, 64.0f);
		s.active = true;
		s.velocity = Random::GenerateDirection() * Random::GenerateFloat(1.0f, 3.0f);
		s.affectedByWind = true;
		s.friction = 0.979f;
		s.gravity = -0.1f;
		s.life = Random::GenerateFloat(80, 120);
		s.destinationSize = Random::GenerateFloat(1024, 1152);
		s.angularVelocity = Random::GenerateFloat(-0.6f, 0.6f);
		s.angularDrag = Random::GenerateFloat(0.87f, 0.99f);
	}

	void SpawnCorpseEffect(const Vector3& pos)
	{
		auto& smoke = GetFreeSmokeParticle();
		smoke = {};

		auto sphere = BoundingSphere(pos, Random::GenerateFloat(8.0f, 16.0f));
		auto spherePos = Random::GeneratePointInSphere(sphere);

		smoke.position = spherePos;
		smoke.sourceColor = Vector4(0.8f, 0.8f, 0.0f, 1.0f);
		smoke.destinationColor = Vector4::Zero;
		smoke.sourceSize = Random::GenerateFloat(32.0f, 64.0f);
		smoke.active = true;
		smoke.velocity = Random::GenerateDirection() * Random::GenerateFloat(0.1f, 0.2f);
		smoke.affectedByWind = true;
		smoke.friction = 0.9f;
		smoke.gravity = 0;;
		smoke.life = Random::GenerateFloat(100.0f, 220.0f);
		smoke.destinationSize = Random::GenerateFloat(BLOCK(1), BLOCK(1.1f));
		smoke.angularVelocity = Random::GenerateFloat(-0.1f, 0.1f);
		smoke.angularDrag = Random::GenerateFloat(0.8f, 0.9f);
	}

	void TriggerBreathSmoke(long x, long y, long z, short angle)
	{
		auto& s = GetFreeSmokeParticle();
		s = {};
		s.position = Vector3(x, y, z) + Vector3(Random::GenerateFloat(8, 16), Random::GenerateFloat(8, 16), Random::GenerateFloat(8, 16));

		float xVel = std::sin(TO_RAD(angle)) * Random::GenerateFloat(8, 12);
		float zVel = std::cos(TO_RAD(angle)) * Random::GenerateFloat(8, 12);
				
		s.velocity = Vector3(xVel, 0, zVel);
		s.sourceColor = Vector4(1, 1, 1, 0.7f);
		s.destinationColor = Vector4(1, 1, 1, 0);
		s.sourceSize = Random::GenerateFloat(8, 24);
		s.active = true;
		s.affectedByWind = true;
		s.friction = 0.999f;
		s.gravity = -0.1f;
		s.life = Random::GenerateFloat(12, 20);
		s.destinationSize = Random::GenerateFloat(128, 160);
		s.angularVelocity = Random::GenerateFloat(-0.5f, 0.5f);
		s.angularDrag = Random::GenerateFloat(0.95f, 0.95f);
	}
}
