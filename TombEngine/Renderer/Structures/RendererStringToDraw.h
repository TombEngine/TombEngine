#pragma once

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

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
