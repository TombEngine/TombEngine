#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererStringToDraw
	{
		Vector2 Position;
		Vector2 PrevPosition;
		int Flags;
		std::wstring String;
		Vector4 Color;
		float Scale;
	};
}
