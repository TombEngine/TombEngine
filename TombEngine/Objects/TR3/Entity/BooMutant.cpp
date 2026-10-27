#include "framework.h"
#include "Objects/TR3/Entity/BooMutant.h"

#include "Game/Animation/Animation.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"

using namespace TEN::Math;

// NOTES:
// BOO_MUTANT is a static trap version of the Seal Mutant.
// It only spits poison gas when triggered and has no AI.

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto BOO_MUTANT_GAS_VEL_MULT = 5.0f;

	const auto BooMutantGasBite = CreatureBiteInfo(Vector3(0.0f, 48.0f, 140.0f), 10);

	static void SpawnBooMutantPoisonGas(ItemInfo& item, float vel)
	{
		constexpr auto GAS_COUNT = 2;

		auto velVector = Vector3(0.0f, 0.0f, vel * BOO_MUTANT_GAS_VEL_MULT);
		auto colorStart = Color(Random::GenerateFloat(0.25f, 0.5f), Random::GenerateFloat(0.25f, 0.5f), 0.1f);
		auto colorEnd = Color(Random::GenerateFloat(0.05f, 0.1f), Random::GenerateFloat(0.05f, 0.1f), 0.0f);

		for (int i = 0; i < GAS_COUNT; i++)
			ThrowPoison(item, BooMutantGasBite, velVector, colorStart, colorEnd, item.ItemFlags[0]);
	}

	void ControlBooMutant(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Status == ITEM_DEACTIVATED)
			return;

		if (item.Status == ITEM_ACTIVE)
		{
			const auto& anim = GetAnimData(item);

			if (TestAnimFrameRange(item, 1, anim.EndFrameNumber - 8))
			{
				float gasVel = item.Animation.FrameNumber - 1;
				if (gasVel > 24.0f)
				{
					gasVel = item.Animation.FrameNumber - (anim.EndFrameNumber - 8);
					if (gasVel <= 0.0f)
						gasVel = 1.0f;

					if (gasVel > 24.0f)
						gasVel = Random::GenerateFloat(8.0f, 24.0f);
				}

				SpawnBooMutantPoisonGas(item, gasVel);
			}

			AnimateItem(&item);
		}
	}
}
