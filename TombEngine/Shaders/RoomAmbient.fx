#include "./CBCamera.hlsli"
#include "./VertexInput.hlsli"
#include "./VertexEffects.hlsli"
#include "./Blending.hlsli"
#include "./Math.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./Shadows.hlsli"
#include "./ShaderLight.hlsli"
#include "./CBStatic.hlsli"

cbuffer RoomBuffer : register(b5)
{
    int Water;
    int Caustics;
    int NumRoomLights;
    int Padding;
    float2 CausticsStartUV;
    float2 CausticsScale;
    float4 AmbientColor;
    ShaderLight RoomLights[MAX_LIGHTS_PER_ROOM];
};

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float2 UV: TEXCOORD0;
	float4 Color: COLOR;
	float ClipDepth : TEXCOORD1;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

// DPDepth-vertex-shader
PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	// Transform vertex to DP-space
	output.Position = mul(float4(input.Position, 1.0f), DualParaboloidView);
	output.Position /= output.Position.w;

	// For the back-map z has to be inverted
	output.Position.z *= Emisphere;

	float L = length(output.Position.xyz);

	output.Position /= L;

	output.ClipDepth = output.Position.z;

	output.Position.x /= output.Position.z + 1.0f;
	output.Position.y /= output.Position.z + 1.0f;

	// Set z for z-buffering and neutralize w
	output.Position.z = (L - NearPlane) / (FarPlane - NearPlane);
	output.Position.w = 1.0f;

	output.UV = input.UV;
	output.Color = input.Color;

	return output;
}

PixelShaderInput VSSky(VertexShaderInput input)
{
	PixelShaderInput output;

	// Transform vertex to DP-space
	output.Position = mul(mul(float4(input.Position, 1.0f), World), DualParaboloidView);
	output.Position /= output.Position.w;

	// For the back-map z has to be inverted
	output.Position.z *= Emisphere;

	float L = length(output.Position.xyz);

	output.Position /= L;

	output.ClipDepth = output.Position.z;

	output.Position.x /= output.Position.z + 1.0f;
	output.Position.y /= output.Position.z + 1.0f;

	// Set z for z-buffering and neutralize w
	output.Position.z = (L - NearPlane) / (FarPlane - NearPlane);
	output.Position.w = 1.0f;

	output.UV = input.UV;
	output.Color = Color;

	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET0
{
	float4 output = Texture.Sample(Sampler, input.UV);

	clip(input.ClipDepth);

	DoAlphaTest(output);

	output.xyz *= input.Color.xyz;

	return output;
}