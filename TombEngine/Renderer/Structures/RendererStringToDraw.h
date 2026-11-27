#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Single line of text with its alignment already applied; Position and PrevPosition are in screen pixels and
	// Flags are PrintStringFlags bits.
	struct RendererStringToDraw
	{
		Vector2 Position;
		Vector2 PrevPosition;
		int Flags;
		std::string String;
		Vector4 Color;
		float Scale;
	};
}
