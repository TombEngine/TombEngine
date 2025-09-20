#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererRoomNode
	{
		short From;
		short To;
		Vector4 ClipPort;
	};
}