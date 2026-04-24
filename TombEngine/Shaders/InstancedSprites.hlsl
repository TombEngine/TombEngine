#include "./CBCamera.hlsli"
#include "./CBAtmosphericSky.hlsli"
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

Texture2D DepthTexture : register(t6);
SamplerState DepthSampler : register(s6);

// Cloud render targets for per-pixel star occlusion.
// Only bound during the star draw call; null bindings return (0,0,0,0) � no occlusion.
Texture2D CloudRenderTargetA : register(t10);
Texture2D CloudRenderTargetB : register(t11);

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

float4 PS(PixelShaderInput input) : SV_TARGET
{
    float4 output = Texture.Sample(Sampler, input.UV) * input.Color;

    InstancedSprite sprite = Sprites[input.InstanceID];

    // Sky-object-only effects (RenderType == 3: stars and meteors).
    if (sprite.RenderType == 3)
    {

    // Moon disk occlusion: discard star/meteor pixels that fall inside the moon.
    // Reconstruct world-space view direction from clip-space position.
    // PositionCopy is the VS clip-space output (before rasterizer), so
    // PositionCopy.y/w > 0 already means "top of screen" � NO Y flip is needed
    // (unlike the atmosphere shader which starts from UV space where y=0 is top).
    if (AtmoMoonEnabled > 0.5f && AtmoMoonVisibility > 0.01f)
    {
        float2 ndc    = input.PositionCopy.xy / input.PositionCopy.w;
        float4 vPos   = mul(float4(ndc.x, ndc.y, 1.0f, 1.0f), InverseProjection);
        vPos.xyz     /= vPos.w;
        float3 viewDir = normalize(mul(float4(vPos.xyz, 0.0f), InverseView).xyz);
        if (dot(viewDir, normalize(AtmoMoonDirection)) > AtmoMoonDiskCosRadius)
            discard;
    }

    // Per-pixel cloud occlusion: sample cloud render targets at this pixel's screen position.
    // CloudRenderTargetA/B are only bound during the star/meteor draw call.
    // Null bindings return (0,0,0,0), so cloudCoverage = 0 and no attenuation is applied.
    {
        float2 screenUV = float2(
            input.PositionCopy.x / input.PositionCopy.w *  0.5f + 0.5f,
            input.PositionCopy.y / input.PositionCopy.w * -0.5f + 0.5f);
        float covA = CloudRenderTargetA.Sample(Sampler, screenUV).a;
        float covB = CloudRenderTargetB.Sample(Sampler, screenUV).a;
        float cloudCoverage = saturate(covA + covB - covA * covB);
        output.a *= (1.0f - cloudCoverage);
    }
    } // RenderType == 3
	
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
		output.w = min(output.w, fade);
	}

    if (sprite.RenderType == 1)
    {
        float4 rawOutput = Texture.Sample(Sampler, input.UV) * input.Color;
        output = DoLaserBarrierEffect(input.Position, float4(ModulateColor(rawOutput.rgb), rawOutput.a), input.UV, FADE_FACTOR, Frame);
    }

    if (sprite.RenderType == 2)
    {
        float4 rawOutput = Texture.Sample(Sampler, input.UV) * input.Color;
        output = DoLaserBeamEffect(input.Position, float4(ModulateColor(rawOutput.rgb), rawOutput.a), input.UV, FADE_FACTOR, Frame);
    }

	output.xyz *= 1.0f - Luma(input.FogBulbs.xyz);
	output.xyz = saturate(output.xyz);

	output = DoDistanceFogForPixel(output, float4(0.0f, 0.0f, 0.0f, 0.0f), input.DistanceFog);

	return output;
}