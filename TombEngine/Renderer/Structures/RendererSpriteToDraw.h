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
		// Billboards use pos with Width and Height (in world units, already multiplied by Scale) and Rotation (2D);
		// three-dimensional quads use vtx1-vtx4 and the per-corner colors c1-c4, with pos as the center of the corners.
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
		// ConstrainAxis and LookAtAxis are used by the constrained billboard types only.
		Vector3 ConstrainAxis;
		Vector3 LookAtAxis;
		bool SoftParticle;
		SpriteRenderType renderType = SpriteRenderType::Default;
	};
}
