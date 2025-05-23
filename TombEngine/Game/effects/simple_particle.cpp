#include "framework.h"
#include "Game/effects/simple_particle.h"

#include "Game/items.h"
#include "Game/Setup.h"
#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Effects
{
	std::array<SimpleParticle, 15> simpleParticles;

	SimpleParticle& GetFreeSimpleParticle()
	{
		for (auto& part : simpleParticles)
		{
			if (!part.active)
				return part;
		}

		return simpleParticles[0];
	}

	void TriggerSnowmobileSnow(ItemInfo* snowMobile)
	{
		float angle = TO_RAD(snowMobile->Pose.Orientation.y);
		const float angleVariation = Random::GenerateFloat(-10, 10) * RADIAN;
		float x = std::sin(angle + angleVariation);
		float z = std::cos(angle + angleVariation);
		x = x* -500 + snowMobile->Pose.Position.x;
		z = z* -500 + snowMobile->Pose.Position.z;

		SimpleParticle& part = GetFreeSimpleParticle();
		part = {};
		part.active = true;
		part.life = Random::GenerateFloat(8, 14);
		part.room = snowMobile->RoomNumber;
		part.ageRate = Random::GenerateFloat(0.9f, 1.3f);
		float size = Random::GenerateFloat(96, 128);
		part.worldPosition = {x, float(snowMobile->Pose.Position.y) - size / 2 , z};
		part.sequence = ID_SKIDOO_SNOW_TRAIL_SPRITES;
		part.size = Random::GenerateFloat(256, 512);
		part.blendMode = BlendMode::AlphaBlend;
	}

	void TriggerSpeedboatFoam(ItemInfo* boat, Vector3 offset)
	{
		for (float i = -0.5; i < 1; i += 1)
		{
			float size = Random::GenerateFloat(96, 128);
			float angle = TO_RAD(boat->Pose.Orientation.y);
			float angleVariation = i * 2 * 10 * RADIAN;
			float y = float(boat->Pose.Position.y) - size / 2 + offset.y;
			float x = std::sin(angle + angleVariation);
			float z = std::cos(angle + angleVariation);
			x = x * offset.z + z * offset.x + boat->Pose.Position.x;
			z = z * offset.z + x * offset.x + boat->Pose.Position.z;

			auto& part = GetFreeSimpleParticle();
			part = {};
			part.active = true;
			part.life = Random::GenerateFloat(5, 9);
			part.room = boat->RoomNumber;
			part.ageRate = Random::GenerateFloat(0.9f, 1.3f);
			part.worldPosition = { x, y, z };
			part.sequence = ID_MOTORBOAT_FOAM_SPRITES;
			part.size = Random::GenerateFloat(256, 512);
			part.blendMode = BlendMode::Additive;
		}
	}

	void UpdateSimpleParticles()
	{
		for (auto& part : simpleParticles)
		{
			if (!part.active)
				continue;

			part.StoreInterpolationData();

			part.age+= part.ageRate;
			if (part.life < part.age)
				part.active = false;

			int spriteCount = -Objects[part.sequence].nmeshes - 1;
			float normalizedAge = part.age / part.life;
			part.sprite = Lerp(0.0f, spriteCount, normalizedAge);
		}
	}
}
