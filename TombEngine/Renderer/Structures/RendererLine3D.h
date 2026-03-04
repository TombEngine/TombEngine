#pragma once

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererLine3D
	{
		Vector3 Origin = Vector3::Zero;
		Vector3 Target = Vector3::Zero;
		Vector4 Color  = Vector3::Zero;
	};
}
