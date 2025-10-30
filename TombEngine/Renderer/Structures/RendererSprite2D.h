#pragma once

#include "Renderer/Structures/RendererSprite.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	struct RendererDisplaySpriteToDraw
	{
		const RendererSprite* SpritePtr = nullptr;

		Vector2 Position	= Vector2::Zero;
		short	Orientation = 0;
		Vector2 Size		= Vector2::Zero;
		Vector4 Color		= Vector4::Zero;

		int		  Priority	= 0;
		BlendMode BlendMode = BlendMode::AlphaBlend;

		Vector2 AspectCorrection = Vector2::One;
	};
}
