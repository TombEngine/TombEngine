#include "VertexInput.hlsli"
#include "Math.hlsli"

cbuffer HUDBuffer : register(b10)
{
	float4x4 View;
	float4x4 Projection;
};

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float2 UV: TEXCOORD;
	float4 Color: COLOR;
};

cbuffer HUDBarBuffer : register(b11)
{
	float2 BarStartUV;
	float2 BarScale;
	float Percent;
	int Poisoned;
	int Frame;
};

Texture2D Texture : register(t5);
SamplerState Sampler : register(s5);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;
	output.Position = mul(mul(float4(input.Position, 1.0f), View), Projection);
	output.Color = input.Color;
	output.UV = input.UV;
	return output;
}

half4 PSColoredHUD(PixelShaderInput input) : SV_TARGET
{
	return input.Color;
}

half4 PSTexturedHUD(PixelShaderInput input) : SV_TARGET
{
	float2 uv = float2((input.UV.x * BarScale.x) + BarStartUV.x, (input.UV.y * BarScale.y) + BarStartUV.y);
	float4 output = Texture.Sample(Sampler, uv);
	return output;
}

half4 glassOverlay(float2 UVs, half4 originalColor) {
	float y = UVs.y;
	y -= 0.15f;
	y = distance(0.1f, y);
	y = 1 - y;
	y = pow(y, 4);
	y = saturate(y);
	half4 color = originalColor;
	return saturate(lerp(color, (color * 1.6f) + half4(0.4f, 0.4f, 0.4f, 0.0f), y));
}

half4 PSColoredHUDBar(PixelShaderInput input) : SV_TARGET
{
	if (input.UV.x > Percent) {
		discard;
	}
	half4 col = input.Color;
	if (Poisoned)
	{
		float factor = sin(((Frame % 30) / 30.0) * PI2) * 0.5 + 0.5;
		col = lerp(col,half4(214 / 512.0, 241 / 512.0, 18 / 512.0, 1),factor);
	}

	return glassOverlay(input.UV,col);
}

half4 PSTexturedHUDBar(PixelShaderInput input) : SV_TARGET
{
	if (input.UV.x > Percent) {
		discard;
	}
	float2 uv = float2((input.UV.x * BarScale.x) + BarStartUV.x, (input.UV.y * BarScale.y) + BarStartUV.y);
	half4 col = Texture.Sample(Sampler, uv);
	if (Poisoned) {
		float factor = sin(((Frame % 30) / 30.0) * PI2) * 0.5 + 0.5;
		col = lerp(col, half4(214 / 512.0, 241 / 512.0, 18 / 512.0, 1), factor);
	}

	return glassOverlay(input.UV, col);
}