#pragma once

namespace TEN::Renderer::ConstantBuffers
{
	using namespace TEN::Math::Library;

	struct alignas(16) CHUDBuffer
	{
		Matrix View;
		//--
		Matrix Projection;
	};
}
