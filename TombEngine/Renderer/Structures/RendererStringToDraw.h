#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererStringToDraw
	{
		float X;
		float Y;
		int Flags;
		std::wstring String;
		Vector4 Color;
		float Scale;
	};
}
