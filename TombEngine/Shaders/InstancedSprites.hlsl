#include "./CBCamera.hlsli"
#include "./Blending.hlsli"
#include "./VertexInput.hlsli"
#include "./Math.hlsli"
#include "./ShaderLight.hlsli"
#include "./SpriteEffects.hlsli"
#include "./VertexEffects.hlsli"

// NOTE: This shader is used for all opaque or not sorted transparent sprites, that can be instanced for a faster drawing

#define INSTANCED_SPRITES_BUCKET_SIZE 512
#define FADE_FACTOR .789f

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float2 UV: TEXCOORD1;
	float4 Color: COLOR;
	float4 PositionCopy: TEXCOORD2;
	float4 FogBulbs : TEXCOORD3;
	float DistanceFog : FOG;
	uint InstanceID : SV_InstanceID;
};

struct PixelShaderOutput
{
	float4 Color : SV_TARGET0;
};

struct InstancedSprite
{
	float4x4 World;
	float4 UV[2];
	float4 Color;
	float IsBillboard;
    float IsSoftParticle;
    int RenderType;
    int PerVertexColor;
};

cbuffer InstancedSpriteBuffer : register(b13)
{
	InstancedSprite Sprites[INSTANCED_SPRITES_BUCKET_SIZE];
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

PixelShaderInput VS(VertexShaderInput input, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

    InstancedSprite sprite = Sprites[InstanceID];
	
	float4 worldPosition;

    if (sprite.IsBillboard == 1)
	{
        worldPosition = mul(float4(input.Position, 1.0f), sprite.World);
        output.Position = mul(mul(float4(input.Position, 1.0f), sprite.World), ViewProjection);
    }
	else
	{
		worldPosition = float4(input.Position, 1.0f);
		output.Position = mul(float4(input.Position, 1.0f), ViewProjection);
	}
	
    int polyIndex = DecodeIndexInPoly(input.Effects);

	output.PositionCopy = output.Position;
    output.Color = lerp(sprite.Color, input.Color, saturate((float)sprite.PerVertexColor));
    output.UV = float2(sprite.UV[0][polyIndex], sprite.UV[1][polyIndex]);
	output.InstanceID  = InstanceID;

	output.FogBulbs = DoFogBulbsForVertex(worldPosition);
	output.DistanceFog = DoDistanceFogForVertex(worldPosition);

	return output;
}

PixelShaderOutput PS(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.Color = Texture.Sample(Sampler, input.UV) * input.Color;

    InstancedSprite sprite = Sprites[input.InstanceID];
	
    if (sprite.IsSoftParticle == 1)
	{
		float particleDepth = input.PositionCopy.z / input.PositionCopy.w;
		input.PositionCopy.xy /= input.PositionCopy.w;
		float2 texCoord = 0.5f * (float2(input.PositionCopy.x, -input.PositionCopy.y) + 1);
		float sceneDepth = DepthTexture.Sample(DepthSampler, texCoord).x;

		sceneDepth = LinearizeDepth(sceneDepth, NearPlane, FarPlane);
		particleDepth = LinearizeDepth(particleDepth, NearPlane, FarPlane);

		if (particleDepth - sceneDepth > 0.01f)
		{
			discard;
		}

		float fade = (sceneDepth - particleDepth) * 1024.0f;
		output.Color.w = min(output.Color.w, fade);
	}

    if (sprite.RenderType == 1)
    {
        float4 rawOutput = Texture.Sample(Sampler, input.UV) * input.Color;
		output.Color = DoLaserBarrierEffect(input.Position, float4(ModulateColor(rawOutput.rgb), rawOutput.a), input.UV, FADE_FACTOR, Frame);
    }

    if (sprite.RenderType == 2)
    {
        float4 rawOutput = Texture.Sample(Sampler, input.UV) * input.Color;
		output.Color = DoLaserBeamEffect(input.Position, float4(ModulateColor(rawOutput.rgb), rawOutput.a), input.UV, FADE_FACTOR, Frame);
    }

	output.Color.xyz *= 1.0f - Luma(input.FogBulbs.xyz);
	output.Color.xyz = saturate(output.Color.xyz);

	output.Color = DoDistanceFogForPixel(output.Color, float4(0.0f, 0.0f, 0.0f, 0.0f), input.DistanceFog);
	output.Color = ApplyBlendModeColor(output.Color, input.PositionCopy, true);

	return output;
}