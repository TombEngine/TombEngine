#pragma once
#include <array>
#include <d3d11.h>
#include <SimpleMath.h>

namespace TEN::Effects::Explosion
{
	struct ExplosionParticle
	{
		Vector3 pos;
		Vector3 vel;
		Vector4 tint;
		float size;
		float rotation;
		float angularVel;
		float age;
		float life;
		int room;
		int sprite;
		bool active;

		Vector3 oldPos;
		Vector4 oldTint;
		float oldSize;
		float oldRotation;

		void StoreInterpolationData()
		{
			oldPos = pos;
			oldTint = tint;
			oldSize = size;
			oldRotation = rotation;
		}
	};
	extern std::array<ExplosionParticle, 64> explosionParticles;

	void TriggerExplosion(const Vector3& pos, float size, bool triggerSparks, bool triggerSmoke, bool triggerShockwave, int room);
	void UpdateExplosionParticles();
	ExplosionParticle& getFreeExplosionParticle();
	void SpawnExplosionParticle(const Vector3& pos);
}
