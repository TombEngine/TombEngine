#pragma once

enum class LaraWeaponType;
struct ItemInfo;

namespace TEN::Effects::Smoke
{
	struct SmokeParticle
	{
		Vector4 sourceColor;
		Vector4 destinationColor;
		Vector4 color;
		Vector3 position;
		Vector3 velocity;
		int room;
		int sprite;
		float gravity;
		float friction;
		float sourceSize;
		float destinationSize;
		float size;
		float age;
		float life;
		float angularVelocity;
		float angularDrag;
		float rotation;
		float terminalVelocity;
		bool affectedByWind;
		bool active;

		Vector3 PrevPosition = Vector3::Zero;
		float	PrevRotation = 0.0f;
		Vector4 PrevColor	 = Vector4::Zero;
		float	PrevSize	 = 0.0f;

		void StoreInterpolationData()
		{
			PrevPosition = position;
			PrevRotation = rotation;
			PrevColor = color;
			PrevSize = size;
		}
	};
	extern std::array<SmokeParticle, 128> SmokeParticles;

	void UpdateSmokeParticles();
	void DisableSmokeParticles();
	void TriggerFlareSmoke(const Vector3& pos, const Vector3& direction, int life, int room);
	void TriggerGunSmokeParticles(int x, int y, int z, int xv, int yv, int zv, byte initial, LaraWeaponType weaponType, byte count);
	void SpawnGunSmokeParticles(const Vector3& pos, const Vector3& direction, int roomNumber, byte initial, LaraWeaponType weaponType, int count);
	void TriggerQuadExhaustSmoke(int x, int y, int z, short angle, int velocity, int moving);
	void TriggerRocketSmoke(int x, int y, int z);
	void TriggerBreathSmoke(long x, long y, long z, short angle);
	void SpawnCorpseEffect(const Vector3& pos);
}
