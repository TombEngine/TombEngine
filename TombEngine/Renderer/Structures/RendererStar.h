#pragma once
#include <SimpleMath.h>
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererStar
	{
		Vector3 Direction;
		Vector3 Color;
		float Blinking;
		float Scale;
		float Extinction;
	};
}