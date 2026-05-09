#pragma once
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::ConstantBuffers
{
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
		int RenderType;
		int PerVertexColor;
	};

	struct alignas(16) CInstancedSpriteBuffer
	{
		InstancedSprite Sprites[INSTANCED_SPRITES_BUCKET_SIZE];
	};
}