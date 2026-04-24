#pragma once
#include <cstdint>
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

		// Sort key used to cluster draws that share GPU pipeline + material, so the opaque
		// pass can issue them contiguously and avoid per-bucket state flips. Ordered so
		// BlendMode (PSO-affecting) dominates, MaterialIndex breaks ties (same shader,
		// different shader data), and Texture/Animated resolve the rest.
		uint64_t SortKey() const
		{
			uint64_t key = 0;
			key |= (uint64_t)(int)BlendMode       << 48;
			key |= (uint64_t)(MaterialIndex & 0xFFFFFF) << 24;
			key |= (uint64_t)(Animated ? 1u : 0u)  << 23;
			key |= (uint64_t)(Texture & 0x7FFFFF);
			return key;
		}
	};
}
