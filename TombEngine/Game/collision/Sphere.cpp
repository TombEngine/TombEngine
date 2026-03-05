#include "framework.h"
#include "Game/collision/Sphere.h"

#include "Game/Lara/lara.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Specific/level.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"

using namespace TEN::Math;
using namespace TEN::Renderer;

namespace TEN::Collision::Sphere
{
	bool HandleItemSphereCollision(ItemInfo& item0, ItemInfo& item1)
	{
		auto spheres0 = item0.GetSpheres();
		auto spheres1 = item1.GetSpheres();

		item1.TouchBits.ClearAll();

		if (spheres0.empty())
		{
			item0.TouchBits.ClearAll();
			return false;
		}

		// Run through item 0 spheres.
		bool isCollided = false;
		for (int i = 0; i < spheres0.size(); i++)
		{
			// Get sphere 0.
			const auto& sphere0 = spheres0[i];
			if (sphere0.Radius <= 0.0f)
				continue;

			// Run through item 1 spheres.
			for (int j = 0; j < spheres1.size(); j++)
			{
				// Get sphere 1.
				const auto& sphere1 = spheres1[j];
				if (sphere1.Radius <= 0.0f)
					continue;

				// Test sphere collision.
				if (!sphere0.Intersects(sphere1))
					continue;

				// Set touch bits.
				item0.TouchBits.Set(i);
				item1.TouchBits.Set(j);

				isCollided = true;
				break;
			}
		}

		return isCollided;
	}
}
