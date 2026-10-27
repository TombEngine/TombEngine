#pragma once

#include <vector>
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::Structures
{
	struct RendererInputLayoutField
	{
		VertexInputFormat Format;
		int Slot;
		char* Semantic;
	};

	struct RendererInputLayout
	{
		std::vector<RendererInputLayoutField> Fields;
	};
}
