#pragma once

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererPolygon
	{
		Vector3 Centre;
		Vector3 Normal;
		unsigned char Shape;
		int BaseIndex;
	};
}
