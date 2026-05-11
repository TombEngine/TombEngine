#pragma once
#include <array>
#include <SimpleMath.h>
#include "Renderer/Structures/RendererSprite.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererSpriteToDraw
	{
		SpriteType Type;
		RendererSprite* Sprite;
		float Scale;
		Vector3 pos;
		Vector3 vtx1;
		Vector3 vtx2;
		Vector3 vtx3;
		Vector3 vtx4;
		std::array<Vector2, 4> CustomUV = {};
		Vector4 c1;
		Vector4 c2;
		Vector4 c3;
		Vector4 c4;
		Vector4 color;
		float Rotation;
		float Width;
		float Height;
		BlendMode BlendMode;
		Vector3 ConstrainAxis;
		Vector3 LookAtAxis;
		bool SoftParticle;
		bool UseCustomUV = false;
		SpriteRenderType renderType = SpriteRenderType::Default;
	};
}
