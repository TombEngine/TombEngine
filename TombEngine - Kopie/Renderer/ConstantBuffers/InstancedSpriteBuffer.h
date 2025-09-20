#pragma once
#include <SimpleMath.h>
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) InstancedSprite
	{
		Matrix World;
		//--
		Vector4 UV[2];
		//--
		Vector4 Color;
		//--
		float IsBillboard;
		float IsSoftParticle;
	};

	struct alignas(16) CInstancedSpriteBuffer
	{
		InstancedSprite Sprites[INSTANCED_SPRITES_BUCKET_SIZE];
	};
}