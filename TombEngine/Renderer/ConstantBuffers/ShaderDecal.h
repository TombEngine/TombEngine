#pragma once

namespace TEN::Renderer::ConstantBuffers
{
	using namespace TEN::Math::Library;

	struct alignas(16) ShaderDecal
	{
		Vector3 Position;
		int Pattern;
		//----------
		float Radius;
		float Opacity;
		int ShaderDecal_Padding0;
		int ShaderDecal_Padding1;
	};
}