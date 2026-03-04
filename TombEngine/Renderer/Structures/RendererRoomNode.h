#pragma once

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererRoomNode
	{
		short From;
		short To;
		Vector4 ClipPort;
	};
}