// @SDLGPU_RESOURCE_MAP
// VS_UBO: 0=0
// PS_UBO:
// PS_TEX:
// @END_RESOURCE_MAP
#pragma pack_matrix(row_major)

#ifdef VERTEX_SHADER
// VS CBs: Camera(0) -> contiguous b0
#define REG_CB_CAMERA b0
#else
// PS CBs: none
#define REG_CB_CAMERA b0
#endif

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