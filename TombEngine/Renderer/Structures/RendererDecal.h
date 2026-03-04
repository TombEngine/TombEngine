#pragma once

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererDecal
	{
		Vector3 Position;
		int Pattern;
		float Radius;
		float Opacity;
	};
}
