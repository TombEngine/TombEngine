#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	// Portal to a neighbour room, with world space vertices and the per-frame culling state.
	struct RendererDoor
	{
		bool Visited;
		bool InvisibleFromCamera;
		float DotProduct;
		short RoomNumber;
		Vector3 Normal;
		Vector3 CameraToDoor;
		Vector4 AbsoluteVertices[4];
		Vector4 TransformedVertices[4];
	};
}
