#pragma once
#include "Renderer/Graphics/RenderTarget2D.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Renderer::Graphics;

	struct RendererRoomAmbientMap
	{
		short RoomNumber;
		RenderTarget2D Front;
		RenderTarget2D Back;
	};
}