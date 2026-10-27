#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

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