#pragma once
namespace TEN::Renderer::ConstantBuffers
{
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