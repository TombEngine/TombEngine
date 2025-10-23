#include "framework.h"
#include "Game/effects/explosion.h"

#include "Game/effects/effects.h"
#include "Game/effects/spark.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Setup.h"
#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Effects::Explosion
{
	using namespace DirectX::SimpleMath;
	using namespace TEN::Effects::Spark;

	std::array<ExplosionParticle, 64> explosionParticles;

	constexpr float PARTICLE_DISTANCE = 512;

	void TriggerExplosion(const Vector3 & pos, float size, bool triggerSparks, bool triggerSmoke, bool triggerShockwave, int room)
	{
		SpawnExplosionParticle(pos);
		for (int i = 0; i < 3; i++)
		{
			auto particlePos = pos + Vector3(Random::GenerateFloat(-size / 2, size / 2), Random::GenerateFloat(-size / 2, size / 2), Random::GenerateFloat(-size / 2, size / 2));
			SpawnExplosionParticle(particlePos);
		}

		if (triggerSparks)
		{
			auto sparkPos = Vector3i(pos);
			//TriggerExplosionSparks(&sparkPos, room); @TODO
		}

		if (triggerShockwave)
		{
			auto shockPos = Pose(Vector3i(pos));
			TriggerShockwave(&shockPos, 0, size, 64, 32, 32, 32, 30, EulerAngles(rand() & 0xFFFF, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
		}
	}

	void UpdateExplosionParticles()
	{
		for (int i = 0; i < explosionParticles.size(); i++)
		{
			auto& e = explosionParticles[i];

			if (!e.active)
				continue;

			e.StoreInterpolationData();

			e.age++;
			if (e.age > e.life)
			{
				e.active = false;
				continue;
			};

			e.vel *= 0.98f;
			e.pos += e.vel;
			e.angularVel *= 0.98f;
			e.rotation += e.angularVel;
			int numSprites = -Objects[ID_EXPLOSION_SPRITES].nmeshes - 1;
			float normalizedAge = e.age / e.life;
			e.sprite = Lerp(0.0f, numSprites, normalizedAge);
			e.tint = Vector4::Lerp(Vector4(2, 2, 2, 1), Vector4::Zero, normalizedAge);
		}
	}

	ExplosionParticle& getFreeExplosionParticle()
	{
		for (int i = 0; i < explosionParticles.size(); i++)
		{
			if (!explosionParticles[i].active)
				return explosionParticles[i];
		}

		return explosionParticles[0];
	}

	void SpawnExplosionParticle(const Vector3& pos)
	{
		auto& e = getFreeExplosionParticle();
		e = {};
		e.pos = pos;
		const float maxVel = 10;
		e.vel = Vector3(Random::GenerateFloat(-maxVel / 2, maxVel / 2), Random::GenerateFloat(-maxVel / 2, maxVel / 2), Random::GenerateFloat(-maxVel / 2, maxVel / 2));
		e.active = true;
		e.tint = Vector4(1, 1, 1, 1);
		e.life = Random::GenerateFloat(60, 90);
		e.size = Random::GenerateFloat(512, 768);
		e.angularVel = Random::GenerateFloat(-RADIAN, RADIAN);
		e.rotation = Random::GenerateFloat(-0.05f, 0.05f);
	}
}
