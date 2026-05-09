#pragma once
namespace TEN::Renderer::ConstantBuffers
{
	struct alignas(16) CHUDBarBuffer
	{
		Vector2 BarStartUV;
		Vector2 BarScale;
		//--
		float Percent;
		int Poisoned;
		int Frame;
		int CHUDBarBuffer_Padding0;
	};
}