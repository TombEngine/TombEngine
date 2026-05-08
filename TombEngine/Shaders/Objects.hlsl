#include "./Math.hlsli"
#include "./CBCamera.hlsli"
#include "./CBObjects.hlsli"
#include "./ShaderLight.hlsli"
#include "./VertexEffects.hlsli"
#include "./VertexInput.hlsli"
#include "./Blending.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./Shadows.hlsli"
#include "./Materials.hlsli"

// Single object shader for moveables (Items) and instanced statics. Both go through the
// same per-instance Object struct in CBObjects:
//   * Items draw with instance_count=1, populate Objects[0] + Bones at the top of the CB,
//     set Skinned!=0 so the VS bone-blend branch and the PS per-bone-mode branch take the
//     skinned path.
//   * InstancedStatics fill Objects[0..N-1] and leave Skinned=0; the bone-blend / per-bone-
//     mode branches collapse uniformly at the draw-call boundary, no per-pixel cost.

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 WorldPosition: POSITION0;
	float3 Normal: NORMAL;
	float2 UV: TEXCOORD0;
	float4 Color: COLOR;
	float Sheen : SHEEN;
	float4 PositionCopy : TEXCOORD1;
	float4 FogBulbs : TEXCOORD2;
	float DistanceFog : FOG;
	float3 Tangent: TANGENT;
	float3 Binormal : BINORMAL;
	float3 FaceNormal : TEXCOORD3;
	uint InstanceID : SV_InstanceID;
	uint Bone : BONE;
};

struct PixelShaderOutput
{
	float4 Color: SV_TARGET0;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

Texture2D AmbientMapFrontTexture : register(t7);
SamplerState AmbientMapFrontSampler : register(s7);

Texture2D AmbientMapBackTexture : register(t8);
SamplerState AmbientMapBackSampler : register(s8);

PixelShaderInput VS(VertexShaderInput input, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

	// Branch is uniform across the draw call (Skinned is a CB scalar set per-draw), so the
	// GPU collapses it without divergence cost.
	//   0 = Static (no bones), 1 = Rigid (single bone), 2 = Full blend, 3 = Classic blend.
	float4x4 world;
	if (Skinned == 0)
	{
		world = Objects[InstanceID].World;
	}
	else if (Skinned == 1)
	{
		world = mul(Bones[input.BoneIndex[0]], Objects[InstanceID].World);
	}
	else
	{
		float4x4 blended = BlendBoneMatrices(input, Bones, Skinned == 3);
		world = mul(blended, Objects[InstanceID].World);
	}

	float wibble = Wibble(input.Effects, DecodeHash(input.AnimationFrameOffsetIndexHash));
	float3 pos = Move(input.Position, input.Effects, wibble);
	float3 col = Glow(input.Color.xyz, input.Effects, wibble);
	float3 worldPosition = mul(float4(pos, 1.0f), world).xyz;

	output.Position = mul(float4(worldPosition, 1.0f), ViewProjection);
	output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
	output.Color = float4(col, input.Color.w);
	output.Color.w *= Objects[InstanceID].Color.w;
	output.PositionCopy = output.Position;
	output.Sheen = DecodeSheen(input.Effects);
	output.InstanceID = InstanceID;
	output.Bone = input.BoneIndex[0];
	output.WorldPosition = worldPosition;

	output.Normal = normalize(mul(input.Normal.xyz, (float3x3) world).xyz);
	output.Tangent = normalize(mul(input.Tangent.xyz, (float3x3) world).xyz);
	output.Binormal = SafeNormalize(mul(cross(input.Normal.xyz, input.Tangent.xyz), (float3x3) world).xyz);
	output.FaceNormal = normalize(mul(input.FaceNormal.xyz, (float3x3) world).xyz);

	output.FogBulbs = DoFogBulbsForVertex(worldPosition);
	output.DistanceFog = DoDistanceFogForVertex(worldPosition);

	return output;
}

PixelShaderOutput PS(PixelShaderInput input)
{
	PixelShaderOutput output;

	input.UV = ConvertAnimUV(input.UV);

	// Apply parallax mapping
	float3x3 TBNf = float3x3(input.Tangent, input.Binormal, input.FaceNormal);
	input.UV = ParallaxOcclusionMapping(TBNf, input.WorldPosition, input.UV);

	float4 ORSH = ConvertAnimOSRH(ORSHTexture.Sample(ORSHSampler, input.UV));
	float ambientOcclusion = ORSH.x;
	float roughness = ORSH.y;
	float specular = ORSH.z;

	float3 emissive = EmissiveTexture.Sample(EmissiveSampler, input.UV).xyz;

	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = ConvertAnimNormal(UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV)));
	normal = EnsureNormal(mul(normal, TBN), input.WorldPosition);

	float4 tex = Texture.Sample(Sampler, input.UV);
	DoAlphaTest(tex);

	// Material effects
	tex.xyz = CalculateReflections(input.WorldPosition, tex.xyz, normal, specular);

	// Ambient occlusion
	float occlusion = CalculateOcclusion(GetSamplePosition(input.PositionCopy), tex.w);
	occlusion *= ambientOcclusion;

	uint numLights = Objects[input.InstanceID].LightInfo.x;
	// Items (Skinned != Static) use per-bone modes packed in BoneLightModes; statics fall
	// back to the per-instance LightInfo.y. Branch is uniform.
	uint mode = (Skinned != 0)
		? (uint)BoneLightModes[input.Bone / 4][input.Bone % 4]
		: Objects[input.InstanceID].LightInfo.y;
	float3 instanceColor = Objects[input.InstanceID].Color.xyz;
	float3 ambient       = Objects[input.InstanceID].AmbientLight.xyz;

	float3 color = (mode == 0) ?
		CombineLights(
			ModulateColor(ambient),
			ModulateColor(input.Color.xyz * instanceColor),
			tex.xyz,
			input.WorldPosition,
			normal,
			input.Sheen,
			Objects[input.InstanceID].Lights,
			numLights,
			input.FogBulbs.w,
			emissive,
			specular,
			roughness) :
		StaticLight(ModulateColor(input.Color.xyz * instanceColor), tex.xyz, input.FogBulbs.w, emissive);

	// Items use a SHADOWABLE_MASK bit packed into NumLights to gate shadow blending. For
	// statics the mask is always clear, so the lerp is a uniform pass-through.
	float shadowable = step(0.5f, float((numLights & SHADOWABLE_MASK) == SHADOWABLE_MASK));
	float3 shadow = DoShadow(input.WorldPosition, normal, color, -0.5f);
	shadow = DoBlobShadows(input.WorldPosition, shadow);
	color = lerp(color, shadow, shadowable);

	output.Color = saturate(float4(color * occlusion, tex.w));
	output.Color = DoFogBulbsForPixel(output.Color, float4(input.FogBulbs.xyz, 1.0f));
	output.Color = DoDistanceFogForPixel(output.Color, FogColor, input.DistanceFog);
	output.Color.w *= input.Color.w;

	return output;
}
