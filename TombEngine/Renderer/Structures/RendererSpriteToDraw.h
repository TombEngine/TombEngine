#pragma once
#include "Math/SimpleMath.h"
#include "Renderer/Structures/RendererSprite.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Sprite record of the view: a billboard (pos, size, rotation and axes) or a 3D quad (vtx1-vtx4, colors c1-c4).
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
		SpriteRenderType renderType = SpriteRenderType::Default;
	};
}
