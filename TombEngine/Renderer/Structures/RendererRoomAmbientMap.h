#pragma once
#include "Renderer/Graphics/IRenderTarget2D.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Renderer::Graphics;

	struct RendererRoomAmbientMap
	{
		short RoomNumber;
		IRenderTarget2D* Front;
		IRenderTarget2D* Back;
	};
}