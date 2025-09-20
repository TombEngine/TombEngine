#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererLine3D
	{
		Vector3 Origin = Vector3::Zero;
		Vector3 Target = Vector3::Zero;
		Vector4 Color  = Vector3::Zero;
	};
}
