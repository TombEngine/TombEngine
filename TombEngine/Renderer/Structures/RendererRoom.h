#pragma once
#include <vector>
#include "Math/SimpleMath.h"
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/Structures/RendererDecal.h"
#include "Renderer/Structures/RendererDoor.h"
#include "Renderer/Structures/RendererEffect.h"
#include "Renderer/Structures/RendererItem.h"
#include "Renderer/Structures/RendererLight.h"
#include "Renderer/Structures/RendererStatic.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Collision;
	using namespace TEN::Math::Library;

	struct RendererRoom
	{
		bool Visited;
		short RoomNumber;
		Vector4 AmbientLight;
		Vector4 ViewPort;
		std::vector<RendererBucket> Buckets;
		std::vector<RendererLight> Lights;
		std::vector<RendererStatic> Statics;
		std::vector<RendererItem*> ItemsToDraw;
		std::vector<RendererEffect*> EffectsToDraw;
		std::vector<RendererLight*> LightsToDraw;
		std::vector<RendererDecal> Decals;
		std::vector<RendererDoor> Doors;
		BoundingBox BoundingBox;
		std::vector<int> Neighbors;
	};
}
