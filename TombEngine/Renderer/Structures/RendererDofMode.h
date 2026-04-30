#pragma once
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	struct DOFState
	{
		float   Distance = 0.0f;
		float   Range = 0.0f;
		float   Strength = 0.0f;
		DOFMode Mode = DOFMode::None;
	};
}