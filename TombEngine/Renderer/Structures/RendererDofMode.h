#pragma once
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	// Depth of field settings; SetDOF clamps Range to zero or more and Strength to [0, 1].
	struct DOFState
	{
		DOFMode Mode     = DOFMode::None;
		float   Distance = 0.0f;
		float   Range    = 0.0f;
		float   Strength = 0.0f;
	};
}