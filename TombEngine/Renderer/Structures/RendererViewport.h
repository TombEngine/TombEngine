#pragma once
namespace TEN::Renderer::Structures
{
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