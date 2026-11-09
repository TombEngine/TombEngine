#pragma once
#include "Math/SimpleMath.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace TEN::Math::Library;

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
		int PerVertexData; // Color and UV come from the vertices instead of the instance.
	};

	struct alignas(16) CInstancedSpriteBuffer
	{
		InstancedSprite Sprites[INSTANCED_SPRITES_BUCKET_SIZE];
	};
}