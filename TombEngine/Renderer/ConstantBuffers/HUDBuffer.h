#pragma once
namespace TEN::Renderer::ConstantBuffers
{
	struct alignas(16) CHUDBuffer
	{
		Matrix View;
		//--
		Matrix Projection;
	};
}
