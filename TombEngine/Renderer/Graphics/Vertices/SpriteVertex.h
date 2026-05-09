#pragma once
namespace TEN::Renderer
{
	struct RendererSpriteVertex
	{
		TEN::Math::Library::Vector3 Position;
		TEN::Math::Library::Vector2 UV;
	};

	struct RendererSpriteInstanceVertex
	{
		TEN::Math::Library::Matrix World;
		TEN::Math::Library::Vector4 Color;
	};
}
