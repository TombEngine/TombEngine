#include "./Math.hlsli"
#include "./CBCamera.hlsli"
#include "./ShaderLight.hlsli"
#include "./VertexEffects.hlsli"
#include "./VertexInput.hlsli"
#include "./Blending.hlsli"
#include "./CBStatic.hlsli"
#include "./Shadows.hlsli"

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 WorldPosition: POSITION;
	float3 Normal: NORMAL;
	float2 UV: TEXCOORD1;
	float4 Color: COLOR;
	float Sheen : SHEEN;
	float4 PositionCopy: TEXCOORD2;
	float4 FogBulbs : TEXCOORD3;
	float DistanceFog : FOG;
	float3 Tangent: TANGENT;
	float3 Binormal: BINORMAL;
};

struct PixelShaderOutput
{
	float4 Color: SV_TARGET0;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	float wibble = Wibble(input.Effects.xyz, input.Hash);
	float3 pos = Move(input.Position, input.Effects.xyz, wibble);
	float3 col = Glow(input.Color.xyz, input.Effects.xyz, wibble);
	
	float4 worldPosition = (mul(float4(pos, 1.0f), World));

	output.Position = mul(worldPosition, ViewProjection);
	output.UV = input.UV;
	output.WorldPosition = worldPosition;
	output.Color = float4(col, input.Color.w);
	output.Color *= Color;

	output.Normal = normalize(mul(input.Normal, (float3x3)World).xyz);
	output.Tangent = normalize(mul(input.Tangent, (float3x3)World).xyz);
	output.Binormal = normalize(mul(input.Binormal, (float3x3)World).xyz);

	output.FogBulbs = DoFogBulbsForVertex(worldPosition);
	output.DistanceFog = DoDistanceFogForVertex(worldPosition);

	output.PositionCopy = output.Position;
    output.Sheen = input.Effects.w;

	return output;
}

float3 UnpackNormalMap(float4 n)
{
	n = n * 2.0f - 1.0f;
	n.z = saturate(1.0f - dot(n.xy, n.xy));
	return n.xyz;
}

PixelShaderOutput PS(PixelShaderInput input)
{
	PixelShaderOutput output;

	float4 tex = Texture.Sample(Sampler, input.UV);
    DoAlphaTest(tex);

	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
	normal = normalize(mul(normal, TBN));

	float3 color = (LightType == 0) ?
		CombineLights(
			AmbientLight.xyz, 
			input.Color.xyz, 
			tex.xyz, 
			input.WorldPosition, 
			normal, 
			input.Sheen, 
			StaticLights, 
			NumStaticLights,
			input.FogBulbs.w) :
		StaticLight(input.Color.xyz, tex.xyz, input.FogBulbs.w);

	color = DoShadow(input.WorldPosition, normal, color, -0.5f);
	color = DoBlobShadows(input.WorldPosition, color);

	output.Color = float4(color, tex.w);
	output.Color = DoFogBulbsForPixel(output.Color, float4(input.FogBulbs.xyz, 1.0f));
	output.Color = DoDistanceFogForPixel(output.Color, FogColor, input.DistanceFog);
	output.Color.w *= input.Color.w;

	return output;
}
