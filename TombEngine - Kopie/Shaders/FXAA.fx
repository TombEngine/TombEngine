#include "./VertexInput.hlsli"
#include "./Math.hlsli"
#include "./CBPostProcess.hlsli"

#define FXAA_SPAN_MAX	8.0
#define FXAA_REDUCE_MUL 1.0/4.0
#define FXAA_REDUCE_MIN 1.0/64.0

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float2 UV: TEXCOORD;
	float4 Color: COLOR;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

float4 ApplyFXAA(PixelShaderInput input)
{
	float2 add = float2(1.0f, 1.0f) / float2(ViewportWidth, ViewportHeight);

	float3 rgbNW = Texture.Sample(Sampler, input.UV + float2(-add.x, -add.y));
	float3 rgbNE = Texture.Sample(Sampler, input.UV + float2(add.x, -add.y));
	float3 rgbSW = Texture.Sample(Sampler, input.UV + float2(-add.x, add.y));
	float3 rgbSE = Texture.Sample(Sampler, input.UV + float2(add.x, add.y));
	float3 rgbM = Texture.Sample(Sampler, input.UV);

	float lumaNW = Luma(rgbNW);
	float lumaNE = Luma(rgbNE);
	float lumaSW = Luma(rgbSW);
	float lumaSE = Luma(rgbSE);
	float lumaM = Luma(rgbM);

	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	float2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max(
		(lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);

	float rcpDirMin = 1.0f / (min(abs(dir.x), abs(dir.y)) + dirReduce);

	dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
		max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin)) * add;

	float3 rgbA = (1.0f / 2.0f) *
		(Texture.Sample(Sampler, input.UV + dir * (1.0f / 3.0f - 0.5f)) +
			Texture.Sample(Sampler, input.UV + dir * (2.0f / 2.0f - 0.5f)));

	float3 rgbB = rgbA * (1.0f / 2.0f) + (1.0f / 4.0f) *
		(Texture.Sample(Sampler, input.UV + dir * (0.0f / 3.0f - 0.5f)) +
			Texture.Sample(Sampler, input.UV + dir * (3.0f / 3.0f - 0.5f)));

	float lumaB = Luma(rgbB);

	if ((lumaB < lumaMin) || (lumaB > lumaMax))
		return float4(rgbA, 1.0f);
	else
		return float4(rgbB, 1.0f);
}

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	output.Position = float4(input.Position, 1.0f);
	output.Color = input.Color;
	output.UV = input.UV;

	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
	return ApplyFXAA(input);
}