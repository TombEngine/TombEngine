// @SDLGPU_RESOURCE_MAP
// VS_UBO: 0=0
// PS_UBO: 0=0 9=1 12=2
// PS_TEX: 0=0 6=1
// @END_RESOURCE_MAP
#pragma pack_matrix(row_major)

#ifdef VERTEX_SHADER
// VS CBs: Camera(0) -> contiguous b0
#define REG_CB_CAMERA b0
// VS unused CBs -> after VS-used
#define REG_CB_SPRITE b1
#define REG_CB_BLENDING b2
// VS unused textures -> all t0/s0
#define REG_TEX_DEPTH t0
#define REG_SMP_DEPTH s0
#else
// PS CBs: Camera(0), Sprite(9), Blending(12) -> contiguous b0-b2
#define REG_CB_CAMERA b0
#define REG_CB_SPRITE b1
#define REG_CB_BLENDING b2
// PS textures -> contiguous t0-t1
#define REG_TEX_DEPTH t1
#define REG_SMP_DEPTH s1
#endif

#include "./CBCamera.hlsli"
#include "./Blending.hlsli"
#include "./Math.hlsli"
#include "./SpriteEffects.hlsli"
#include "./ShaderLight.hlsli"
#include "./VertexInput.hlsli"

// NOTE: Shader is used for all 3D and alpha blended sprites because transformed vertices are already sent to GPU instead of instances.

#define FADE_FACTOR .789f

cbuffer SpriteBuffer : register(REG_CB_SPRITE)
{
	float IsSoftParticle;
	int RenderType;
};

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 Normal: NORMAL;
	float2 UV: TEXCOORD1;
	float4 Color: COLOR;
	float4 PositionCopy: TEXCOORD2;
	float4 FogBulbs : TEXCOORD3;
	float DistanceFog : FOG;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D DepthTexture : register(REG_TEX_DEPTH);
SamplerState DepthSampler : register(REG_SMP_DEPTH);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	float4 worldPosition = float4(input.Position, 1.0f);

	output.Position = mul(worldPosition, ViewProjection);
	output.PositionCopy = output.Position;
    output.Normal = input.Normal.xyz;
	output.Color = input.Color;
	output.UV = input.UV;

	output.FogBulbs = DoFogBulbsForVertex(worldPosition);
	output.DistanceFog = DoDistanceFogForVertex(worldPosition);

	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
	float4 output = Texture.Sample(Sampler, input.UV) * input.Color;

	if (IsSoftParticle == 1)
	{
		float particleDepth = input.PositionCopy.z / input.PositionCopy.w;
		input.PositionCopy.xy /= input.PositionCopy.w;

		float2 texCoord = 0.5f * (float2(input.PositionCopy.x, -input.PositionCopy.y) + 1.0f);
		float sceneDepth = DepthTexture.Sample(DepthSampler, texCoord).x;

		sceneDepth = LinearizeDepth(sceneDepth, NearPlane, FarPlane);
		particleDepth = LinearizeDepth(particleDepth, NearPlane, FarPlane);

		if (particleDepth - sceneDepth > 0.01f)
			discard;

		float fade = (sceneDepth - particleDepth) * 1024.0f;
		output.w = min(output.w, fade);
	}

	if (RenderType == 1)
	{
		output = DoLaserBarrierEffect(input.Position, output, input.UV, FADE_FACTOR, Frame);
	}

	if (RenderType == 2)
	{
		output = DoLaserBeamEffect(input.Position, output, input.UV, FADE_FACTOR, Frame);
	}

	output.xyz *= 1.0f - Luma(input.FogBulbs.xyz);
	output.xyz = saturate(output.xyz);

	output = DoDistanceFogForPixel(output, float4(0.0f, 0.0f, 0.0f, 0.0f), input.DistanceFog);

	return output;
}
