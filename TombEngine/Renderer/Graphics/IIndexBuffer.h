#pragma once
#include <vector>
#include "Specific/Structures/fast_vector.h"

namespace TEN::Renderer::Graphics
{
	class IIndexBuffer
	{
	public:
		virtual ~IIndexBuffer() = default;
	};
}
