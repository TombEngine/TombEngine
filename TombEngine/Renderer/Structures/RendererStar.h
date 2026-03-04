#pragma once
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererStar
	{
		Vector3 Direction;
		Vector3 Color;
		float Blinking;
		float Scale;
		float Extinction;
	};
}