#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Debug line in world space.
	struct RendererLine3D
	{
		Vector3 Origin = Vector3::Zero;
		Vector3 Target = Vector3::Zero;
		Vector4 Color  = Vector3::Zero;
	};
}
