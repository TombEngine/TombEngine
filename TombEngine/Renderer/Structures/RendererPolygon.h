#pragma once
namespace TEN::Renderer::Structures
{
	struct RendererPolygon
	{
		Vector3 Centre;
		Vector3 Normal;
		unsigned char Shape;
		int BaseIndex;
	};
}
