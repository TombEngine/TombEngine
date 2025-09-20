#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer
{
	struct RendererSpriteVertex
	{
		DirectX::SimpleMath::Vector3 Position;
		DirectX::SimpleMath::Vector2 UV;
	};

	struct RendererSpriteInstanceVertex
	{
		DirectX::SimpleMath::Matrix World;
		DirectX::SimpleMath::Vector4 Color;
	};
}
