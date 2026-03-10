#include "./CBCamera.hlsli"
#include "./VertexInput.hlsli"

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float4 Color: COLOR;
};

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	output.Position = mul(float4(input.Position, 1.0f), ViewProjection);
	output.Color = input.Color;

	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
	return input.Color;
}