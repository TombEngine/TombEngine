#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Debug text queued by gameplay code and drawn by the ImGui overlay. Position is in display space.
	struct RendererDebugLabel
	{
		std::string String;
		Vector2 Position;
		Vector4 Color;
		float Scale;
	};
}
