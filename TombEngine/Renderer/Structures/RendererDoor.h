#pragma once

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

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
