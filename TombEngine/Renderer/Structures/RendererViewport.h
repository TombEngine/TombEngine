#pragma once

namespace TEN::Renderer::Structures
{
	struct RendererViewport
	{
		int X;
		int Y;
		int Width;
		int Height;
		float MinDepth = 0.0f;
		float MaxDepth = 1.0f;
	};
}
