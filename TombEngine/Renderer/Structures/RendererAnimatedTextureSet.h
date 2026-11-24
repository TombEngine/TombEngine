#pragma once
#include <vector>
#include "Renderer/Structures/RendererAnimatedTexture.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Frames cycles through the frames and UVRotate scrolls the UV; Video marks a video texture.
	enum class AnimatedTextureType
	{
		Frames,
		UVRotate,
		Video
	};

	// Animated texture sequence converted from the level data on load; Fps and the UV rotate fields apply to the matching type only.
	struct RendererAnimatedTextureSet
	{
		AnimatedTextureType Type = AnimatedTextureType::Frames;
		int Fps = 0;
		float UVRotateSpeed = 0.0f;
		float UVRotateDirection = 0.0f;
		std::vector<RendererAnimatedTexture> Textures;
	};
}
