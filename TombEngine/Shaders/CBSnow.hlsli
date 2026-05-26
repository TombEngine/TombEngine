#ifndef CB_SNOW
#define CB_SNOW

// Constant buffer for the deformable snow overlay pass.
// Bound to slot b6 (previously CBAnimatedTexture, now unused).
// Must match CSnowBuffer in Renderer/ConstantBuffers/SnowBuffer.h.

cbuffer CBSnow : register(b6)
{
	// World-space XZ position the heightmap is centered on (player position, quantized
	// to whole heightmap pixels).
	float2 SnowCentre;

	// World-space radius covered by the heightmap. The full grid spans 2*radius units.
	float  SnowWorldRadius;

	// Maximum vertical deformation depth in world units. Mesh is lifted by this
	// amount at rest; full-stamped (h=1) returns to original floor level.
	float  SnowMaxDepth;
	//--

	// Snow surface tint (RGB) plus rim highlight strength (A).
	float4 SnowTintAndRim;

	// Procedural micro-hill parameters: x = amplitude (world units, down-direction),
	// y = spatial frequency (rad/unit), zw = reserved.
	float4 SnowHillParams;
};

#endif
