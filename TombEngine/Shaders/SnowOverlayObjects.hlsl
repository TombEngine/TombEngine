// ============================================================================
// SnowOverlayObjects.hlsl - Deformable snow overlay pass for moveables and
// instanced statics.
//
// Same idea as SnowOverlay.hlsl (rooms) but the vertex is first transformed
// from object/mesh-local space to world space via the per-instance Object CB
// world matrix. The snow lift and heightmap deformation are then applied in
// world-Y so the blanket always rises upward regardless of the object's
// orientation. Skirt-bottom vertices are tagged with Color.a = 0 so they stay
// pinned to the original surface position (no lift, no deformation).
//
// Skinning is intentionally unsupported (Skinned is forced to 0 by the C++
// draw path): the overlay geometry is baked in bind pose and bone-deforming
// it would shear the snow blanket.
// ============================================================================

#include "./CBCamera.hlsli"
#include "./CBSnow.hlsli"
#include "./CBObjects.hlsli"
#include "./Math.hlsli"
#include "./Blending.hlsli"
#include "./ShaderLight.hlsli"
#include "./Shadows.hlsli"
#include "./VertexInput.hlsli"

Texture2D    SnowSurface     : register(t0);
SamplerState SnowSurfaceSamp : register(s0);

Texture2D    SnowHeightmap   : register(t16);
SamplerState SnowFieldSamp   : register(s0);

struct PixelShaderInput
{
	float4 Position      : SV_POSITION;
	float3 WorldPosition : POSITION0;
	float3 Normal        : NORMAL;
	float4 Color         : COLOR;
	float2 UV            : TEXCOORD0;
	float  Depth01       : TEXCOORD1;
	float4 FogBulbs      : TEXCOORD2;
	float  DistanceFog   : FOG;
	uint   InstanceID    : SV_InstanceID;
};

float SampleSnowDepth(float worldX, float worldZ)
{
	float2 origin = SnowCentre - float2(SnowWorldRadius, SnowWorldRadius);
	float2 uv     = (float2(worldX, worldZ) - origin) / (SnowWorldRadius * 2.0f);

	if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
		return 0.0f;

	return SnowHeightmap.SampleLevel(SnowFieldSamp, uv, 0).r;
}

PixelShaderInput VS(VertexShaderInput input, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

	// Per-vertex lift mask baked at mesh-gen time:
	//   1 = overlay surface / skirt top  -> gets full world-Y lift + heightmap deformation.
	//   0 = skirt bottom                 -> pinned to original surface position.
	float liftScale = input.Color.a;

	float4x4 world = Objects[InstanceID].World;
	float3 worldPos = mul(float4(input.Position, 1.0f), world).xyz;

	// Heightmap sampled at world XZ so trails on items/statics align with the
	// world-space deformation grid (same as rooms).
	float h = SampleSnowDepth(worldPos.x, worldPos.z);

	float hillFreq   = SnowHillParams.y;
	float hillHeight = SnowHillParams.x;
	float n  = sin(worldPos.x * hillFreq) * cos(worldPos.z * hillFreq * 0.83f);
	float n2 = sin(worldPos.x * hillFreq * 2.17f + 1.3f) * cos(worldPos.z * hillFreq * 1.71f - 0.7f);
	float hill01    = saturate((n * 0.5f + n2 * 0.25f) + 0.375f);
	float hillFade  = saturate(1.0f - h);
	float hillPushY = hill01 * hillHeight * hillFade;

	float totalLift = SnowMaxDepth + hillHeight;

	// World-Y is down in TombEngine, so "lift up" subtracts from Y.
	// Order: raise to snow surface, apply debug offset, then push back down by
	// (heightmap depth + hills) so footprints flatten to the original surface.
	worldPos.y -= totalLift * liftScale;
	worldPos.y -= SnowHillParams.z * liftScale;
	worldPos.y += (h * totalLift + hillPushY) * liftScale;

	output.Position      = mul(float4(worldPos, 1.0f), ViewProjection);
	output.WorldPosition = worldPos;
	// Transform normal by the upper 3x3 of the object world matrix.
	output.Normal        = normalize(mul(input.Normal.xyz, (float3x3)world));
	output.Color         = input.Color;
	output.UV            = input.UV;
	output.Depth01       = h;
	output.FogBulbs      = DoFogBulbsForVertex(worldPos);
	output.DistanceFog   = DoDistanceFogForVertex(worldPos);
	output.InstanceID    = InstanceID;
	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET0
{
	float4 base = SnowSurface.Sample(SnowSurfaceSamp, input.UV);
	DoAlphaTest(base);

	float3 tint  = SnowTintAndRim.rgb;
	float  trail = saturate(input.Depth01);
	float3 col   = lerp(base.rgb * tint, base.rgb, trail);

	float gradMag = length(float2(ddx(input.Depth01), ddy(input.Depth01)));
	float rim     = saturate(gradMag * 8.0f) * SnowTintAndRim.a;
	col          += tint * rim;

	// Lighting via the per-object light array bound by BindMoveableLights /
	// BindInstancedStaticLights. Uses the same CombineLights model as Objects.hlsl
	// so the snow blanket on items/statics matches the surrounding object shading.
	float3 normal = normalize(input.Normal);
	uint   numLights    = Objects[input.InstanceID].LightInfo.x;
	float3 instanceCol  = Objects[input.InstanceID].Color.xyz;
	float3 ambient      = Objects[input.InstanceID].AmbientLight.xyz;

	float3 vertCol = input.Color.xyz * instanceCol;

	float3 lit = CombineLights(
		ModulateColor(ambient),
		ModulateColor(vertCol),
		col,
		input.WorldPosition,
		normal,
		0.0f, // sheen
		Objects[input.InstanceID].Lights,
		(int)numLights,
		input.FogBulbs.w,
		float3(0.0f, 0.0f, 0.0f), // emissive
		0.0f,                     // specular
		1.0f);                    // roughness

	float4 outColor;
	outColor.rgb = lit;
	outColor.a   = base.a;

	outColor = DoFogBulbsForPixel(outColor, float4(input.FogBulbs.xyz, 1.0f));
	outColor = DoDistanceFogForPixel(outColor, FogColor, input.DistanceFog);
	return outColor;
}
