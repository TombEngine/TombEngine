// ============================================================================
// SnowOverlay.hlsl - Deformable snow surface pass.
//
// Renders Phase 2 snow overlay buckets with vertex-level deformation read from
// the per-frame heightmap (SnowField). The undeformed mesh sits SnowMaxDepth
// above the original floor (lift baked in at mesh-gen time). The VS samples
// the heightmap at the vertex's world XZ and pushes the vertex back down by
// (h * SnowMaxDepth), so freshly stamped pixels are flush with the floor while
// untouched cells stay at full snow depth.
// ============================================================================

#include "./CBCamera.hlsli"
#include "./CBSnow.hlsli"
#include "./VertexInput.hlsli"

Texture2D    SnowSurface     : register(t0);
SamplerState SnowSurfaceSamp : register(s0); // Sampler bound at same slot as ColorMap.

Texture2D    SnowHeightmap   : register(t16); // SRV at slot 16 (valid; D3D11 supports up to 128).
SamplerState SnowFieldSamp   : register(s0);  // Sampler at slot 0 (D3D11 max is 15; BindTextureToStage wraps slot % 16).

struct PixelShaderInput
{
	float4 Position : SV_POSITION;
	float2 UV       : TEXCOORD0;
	float  Depth01  : TEXCOORD1; // sampled heightmap value at the vertex
};

float SampleSnowDepth(float worldX, float worldZ)
{
	// Map world XZ to [0,1] within the heightmap's coverage. Outside-circle
	// samples clamp to 0 thanks to the clamp sampler + cleared edges in Recenter.
	float2 origin = SnowCentre - float2(SnowWorldRadius, SnowWorldRadius);
	float2 uv     = (float2(worldX, worldZ) - origin) / (SnowWorldRadius * 2.0f);

	if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
		return 0.0f;

	return SnowHeightmap.SampleLevel(SnowFieldSamp, uv, 0).r;
}

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	// Per-vertex lift scale (baked at mesh-gen time): 1 = full snow thickness above
	// floor, 0 = vertex flush with floor with no deformation. Used to roll the snow
	// surface smoothly down to the floor at drop edges and to keep skirt bottoms
	// pinned to the neighbor floor regardless of trodden state.
	float liftScale = input.Color.a;

	float h = SampleSnowDepth(input.Position.x, input.Position.z);

	// Procedural micro-hills: two-axis sinusoidal noise in [0..1] gives gentle
	// undulating mounds without any extra texture lookup. Fade out as the snow
	// is deformed so footprints/explosions still flatten cleanly to the floor.
	float hillFreq   = SnowHillParams.y;
	float hillHeight = SnowHillParams.x;
	float n  = sin(input.Position.x * hillFreq) * cos(input.Position.z * hillFreq * 0.83f);
	float n2 = sin(input.Position.x * hillFreq * 2.17f + 1.3f) * cos(input.Position.z * hillFreq * 1.71f - 0.7f);
	float hill01    = saturate((n * 0.5f + n2 * 0.25f) + 0.375f); // remap to ~[0..1].
	float hillFade  = saturate(1.0f - h);
	float hillPushY = hill01 * hillHeight * hillFade;

	// Push deformed vertex back down toward original floor. Y is down in TEN, so
	// "down" means increasing Y. Mesh was lifted by (MaxDepth + HillHeight) * liftScale
	// at gen time so hill peaks can rise above the standard snow line; the (h * total)
	// term guarantees a fully stamped sample (h=1) lands flush with the floor.
	float totalLift = SnowMaxDepth + hillHeight;

	float3 worldPos = input.Position;
	worldPos.y += (h * totalLift + hillPushY) * liftScale;

	output.Position = mul(float4(worldPos, 1.0f), ViewProjection);
	output.UV       = input.UV;
	output.Depth01  = h;
	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET0
{
	float4 base = SnowSurface.Sample(SnowSurfaceSamp, input.UV);

	// Tint snow surface: full tint where pristine (h=0), fade to plain base
	// texture where fully deformed (h=1) so trails reveal the ground colour.
	float3 tint  = SnowTintAndRim.rgb;
	float  trail = saturate(input.Depth01);
	float3 col   = lerp(base.rgb * tint, base.rgb, trail);

	// Cheap rim highlight at the boundary between deformed and pristine snow.
	// Stub for Phase 6 - derivatives give a 1px-wide outline along trails.
	float gradMag = length(float2(ddx(input.Depth01), ddy(input.Depth01)));
	float rim     = saturate(gradMag * 8.0f) * SnowTintAndRim.a;
	col          += tint * rim;

	return float4(col, base.a);
}
