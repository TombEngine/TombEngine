#pragma once
#include <d3d11.h>
#include <SimpleMath.h>
#include "Math/Math.h"

namespace TEN::Effects::Spark
{
	constexpr auto SPARK_RICOCHET_COLOR_DEFAULT = Vector4(1.0f, 1.0f, 0.0f, 1.0f);

	struct SparkParticle
	{
		Vector3 pos;
		Vector3 velocity;
		Vector4 sourceColor;
		Vector4 destinationColor;
		Vector4 color;
		int room;
		float gravity;
		float friction;
		float age;
		float life;
		float width;
		float height;
		bool active;

		Vector3 PrevPosition = Vector3::Zero;
		Vector3 PrevVelocity = Vector3::Zero;

		void StoreInterpolationData()
		{
			PrevPosition = pos;
			PrevVelocity = velocity;
		}
	};
	extern std::array<SparkParticle, 128> SparkParticles;
			
	void UpdateSparkParticles();
	void DisableSparkParticles();
	SparkParticle& GetFreeSparkParticle();

	void TriggerFlareSparkParticles(const Vector3i& pos, const Vector3i& vel, const Color& color, int roomNumber);
	void TriggerRicochetSpark(const GameVector& pos, short angle, int num, const Vector4& colorStart = SPARK_RICOCHET_COLOR_DEFAULT);
	void TriggerFrictionSpark(const GameVector& pos, const EulerAngles& angle, float length, int count);
	void TriggerElectricSpark(const GameVector& pos, const EulerAngles& angle, int count);
	void TriggerAttackSpark(const Vector3& basePos, const Vector3& color);
	void SpawnCyborgSpark(const Vector3& pos);
}
