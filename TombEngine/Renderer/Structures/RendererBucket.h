#pragma once
#include <vector>
#include <SimpleMath.h>
#include "Renderer/RendererEnums.h"
#include "Renderer/Structures/RendererPolygon.h"

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererBucket
	{
		int Texture;
		bool Animated;
		BlendMode BlendMode;
		int MaterialIndex;
		int StartVertex;
		int StartIndex;
		int NumVertices;
		int NumIndices;
		Vector3 Centre;
		std::vector<RendererPolygon> Polygons;

		// Snow deformation overlay bucket. Geometry generated at level load for sectors with
		// MaterialType::Snow. Drawn in a separate pass with the snow shader and skipped by the
		// regular DrawRooms loop. See Game/effects/SnowField.* and Renderer/RendererSnowField.*.
		bool IsSnowOverlay = false;
	};
}
