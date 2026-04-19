#include "./CBCamera.hlsli"
#include "./CBItem.hlsli"
#include "./Blending.hlsli"
#include "./VertexInput.hlsli"
#include "./ShaderLight.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./VertexEffects.hlsli"
#include "./Materials.hlsli"

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 Normal: NORMAL0;
	float3 WorldPosition : POSITION;
	float2 UV: TEXCOORD;
	float4 Color: COLOR;
    float Sheen : SHEEN;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float3 FaceNormal : NORMAL1;
};

struct PixelShaderOutput
{
    float4 Color : SV_Target0;
    float4 Emissive : SV_Target1;
};
    
Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

    float4x4 blended = Skinned ? BlendBoneMatrices(input, Bones, (Skinned == 2)) : Bones[input.BoneIndex[0]];
    float4x4 world = mul(blended, World);

	output.Position = mul(mul(float4(input.Position, 1.0f), world), ViewProjection);
    output.Normal = (mul(input.Normal.xyz, (float3x3) world).xyz);
    output.Tangent = normalize(mul(input.Tangent.xyz, (float3x3) world).xyz);
    output.Binormal = SafeNormalize(mul(cross(input.Normal.xyz, input.Tangent.xyz), (float3x3) world).xyz);
    output.Color = input.Color;
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
    output.WorldPosition = (mul(float4(input.Position, 1.0f), world).xyz);
    output.Sheen = DecodeSheen(input.Effects);
    output.FaceNormal = normalize(mul(input.FaceNormal.xyz, (float3x3) world).xyz);
    
	return output;
}

PixelShaderOutput PS(PixelShaderInput input) : SV_TARGET
{
	if (Animated && Type == 1)
        input.UV = CalculateUVRotate(input.UV, 0);
	
    PixelShaderOutput output;
    
    float4 tex = Texture.Sample(Sampler, input.UV);
    float3 baseColor = tex.xyz * ModulateColor(Color.xyz);
    float3 pos = normalize(input.WorldPosition);

    output.Color = float4(baseColor, tex.w * Color.w);

    DoAlphaTest(output.Color);
    
    float4 ORSH = ORSHTexture.Sample(ORSHSampler, input.UV);
    float ambientOcclusion = ORSH.x;
    float roughness = ORSH.y;
    float specular = ORSH.z;
	
    float3 emissive = EmissiveTexture.Sample(EmissiveSampler, input.UV).xyz;
	
    float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
    float3 normal = UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
    normal = normalize(mul(normal, TBN));
    
    // Material effects
    output.Color.xyz = CalculateReflections(input.WorldPosition, output.Color.xyz, normal, specular);

    ShaderLight l;
    l.Intensity = 0.3f;
    l.Type = LT_SUN;
    l.Direction = normalize(float3(-1.0f, -0.707f, -0.5f));
    l.Color.xyz = ModulateColor(AmbientLight.xyz);

    float3 lighting = DoDirectionalLight(pos, normal, l);
    lighting += DoSpecularSun(normal, l, input.Sheen, specular, roughness);
    lighting += emissive;

     // Emissive material
    output.Color.xyz += lighting * output.Color.a;
    output.Color.xyz = saturate(output.Color.xyz);
    
    output.Emissive = float4(emissive, 1.0f);
	
	output.Color.xyz = GammaCorrection(output.Color.xyz);
    return output;
}
