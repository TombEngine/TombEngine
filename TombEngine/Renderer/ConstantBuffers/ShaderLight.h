#pragma once

namespace TEN::Renderer::ConstantBuffers
{
	using namespace TEN::Math::Library;

	struct alignas(16) ShaderLight
	{
		Vector3 Position;
		unsigned int Type;
		Vector3 Color;
		float Intensity;
		Vector3 Direction;
		float In;
		float Out;
		float InRange;
		float OutRange;
		int ShaderLight_Padding0;
	};
}