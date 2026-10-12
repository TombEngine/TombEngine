#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererViewport
	{
		int X;
		int Y;
		int Width;
		int Height;
		float MinDepth;
		float MaxDepth;
	};
}