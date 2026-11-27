#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Decal visible in a room this frame, copied from the game decal; Pattern is the decal type and Position is the sphere center.
	struct RendererDecal
	{
		Vector3 Position;
		int Pattern;
		float Radius;
		float Opacity;
	};
}
