#include "./CBCamera.hlsli"
#include "./Blending.hlsli"
#include "./VertexInput.hlsli"
#include "./ShaderLight.hlsli"
#include "./AnimatedTextures.hlsli"

cbuffer ItemBuffer : register(b1)
{
	float4x4 World;
	float4x4 Bones[32];
	float4 ItemPosition;
	float4 AmbientLight;
};

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 Normal: NORMAL;
	float3 WorldPosition : POSITION;
	float2 UV: TEXCOORD;
	float4 Color: COLOR;
	float Sheen : SHEEN;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	output.Position = mul(mul(float4(input.Position, 1.0f), World), ViewProjection);
	output.Normal = (mul(float4(input.Normal, 0.0f), World).xyz);
    output.Color = input.Color;
    output.UV = GetUVPossiblyAnimated(input.UV, input.PolyIndex, input.AnimationFrameOffset);
    output.WorldPosition = (mul(float4(input.Position, 1.0f), World).xyz);
	output.Sheen = input.Effects.w;
	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
    if (Animated && Type == 1)
        input.UV = CalculateUVRotate(input.UV, 0);
	
    float4 output = Texture.Sample(Sampler, input.UV);
    float3 normal = normalize(input.Normal);
    float3 pos = normalize(input.WorldPosition);

    DoAlphaTest(output);
	
    ShaderLight l;
    l.Color = float3(1.0f, 1.0f, 0.5f);
    l.Intensity = 0.3f;
    l.Type = LT_SUN;
    l.Direction = normalize(float3(-1.0f, -0.707f, -0.5f));

    output.xyz += DoDirectionalLight(pos, normal, l);
    output.xyz += DoSpecularSun(input.Normal, l, input.Sheen);

	//adding some pertubations to the lighting to add a cool effect
    float3 noise = SimplexNoise(output.xyz);
    output.xyz = NormalNoise(output, noise, normal);
	
    return output;
}
