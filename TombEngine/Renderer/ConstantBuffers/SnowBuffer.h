#pragma once

#include <SimpleMath.h>

// ============================================================================
// SnowBuffer.h - GPU constant buffer for the deformable snow overlay pass.
//
// Bound to register b6 (unused slot, previously CBAnimatedTexture).
// Must match CBSnow.hlsli layout exactly.
// ============================================================================

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) CSnowBuffer
	{
		// Row 0
		Vector2 SnowCentre;      // World-space XZ centre of the heightmap.
		float   SnowWorldRadius; // Half-extent of the heightmap in world units.
		float   SnowMaxDepth;    // Maximum vertical deformation (world units).
		//--
		// Row 1
		Vector4 SnowTintAndRim;  // RGB = tint, A = rim highlight strength.
		//--
		// Row 2
		Vector4 SnowHillParams;  // x = hill height (world units), y = hill frequency (rad/unit), zw = reserved.
	};
}
