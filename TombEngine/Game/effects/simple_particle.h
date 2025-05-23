#pragma once
#include "Objects\objectslist.h"
#include <SimpleMath.h>

enum class BlendMode;
struct ItemInfo;

namespace TEN::Effects
{
	using namespace DirectX::SimpleMath;

	struct SimpleParticle
	{
		Vector3 worldPosition;
		float size;
		float age;
		float ageRate;
		float life;
		int room;
		unsigned int sprite;
		GAME_OBJECT_ID sequence;
		bool active;
		BlendMode blendMode;

		Vector3 PrevWorldPosition = Vector3::Zero;
		float	PrevSize		  = 0.0f;

		void StoreInterpolationData()
		{
			PrevWorldPosition = worldPosition;
			PrevSize = size;
		}
	};
	extern std::array<SimpleParticle, 15> simpleParticles;

	SimpleParticle& GetFreeSimpleParticle();
	void TriggerSnowmobileSnow(ItemInfo* snowMobile);
	void TriggerSpeedboatFoam(ItemInfo* boat, Vector3 offset);
	void UpdateSimpleParticles();
}
