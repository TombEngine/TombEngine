#ifndef BLENDINGSHADER
#define BLENDINGSHADER

#include "./CBBlending.hlsli"
#include "./Math.hlsli"

#define ALPHATEST_NONE			0
#define ALPHATEST_GREATER_THAN	1
#define ALPHATEST_LESS_THAN		2

#define BLENDMODE_OPAQUE	  0
#define BLENDMODE_ALPHATEST	  1
#define BLENDMODE_ADDITIVE	  2
#define BLENDMODE_DISTORTION 3
#define BLENDMODE_NOZTEST	  4
#define BLENDMODE_SUBTRACTIVE 5
#define BLENDMODE_WIREFRAME	  6
#define BLENDMODE_EXCLUDE	  8
#define BLENDMODE_SCREEN	  9
#define BLENDMODE_LIGHTEN	  10
#define BLENDMODE_ALPHABLEND  11

#define ZERO	   float3(0.0f, 0.0f, 0.0f)
#define EIGHT_FIVE float3( 0.85f, 0.85f, 0.85f)
#define BLENDING   0.707f

Texture2D DepthTexture : register(t6);
SamplerState DepthSampler : register(s6);

inline bool BlendModeSupportsSSAO()
{
    return (BlendMode == BLENDMODE_OPAQUE || BlendMode == BLENDMODE_ALPHATEST || BlendMode == BLENDMODE_ALPHABLEND);
}

void DoAlphaTest(float4 inputColor)
{
	if (AlphaTest == ALPHATEST_GREATER_THAN && inputColor.w < AlphaThreshold)
	{
		discard;
	}
	else if (AlphaTest == ALPHATEST_LESS_THAN && inputColor.w > AlphaThreshold)
	{
		discard;
	}
	else
	{
		return;
	}
}

float4 DoDistanceFogForPixel(float4 sourceColor, float4 fogColor, float value)
{
	switch (BlendMode)
	{
		case BLENDMODE_ADDITIVE:
		case BLENDMODE_DISTORTION:
		case BLENDMODE_SCREEN:
		case BLENDMODE_LIGHTEN:
			fogColor.xyz *= Luma(sourceColor.xyz);
			break;

		case BLENDMODE_SUBTRACTIVE:
		case BLENDMODE_EXCLUDE:
			fogColor.xyz *= 1.0f - Luma(sourceColor.xyz);
			break;

		case BLENDMODE_ALPHABLEND:
			fogColor.w = sourceColor.w;
			break;

		default:
			break;
	}

	if (fogColor.w > sourceColor.w)
		fogColor.w = sourceColor.w;

	float4 result = lerp(sourceColor, fogColor, value);
	return result;
}

float4 DoFogBulbsForPixel(float4 sourceColor, float4 fogColor)
{
	switch (BlendMode)
	{
		case BLENDMODE_ADDITIVE:
		case BLENDMODE_DISTORTION:
		case BLENDMODE_SCREEN:
		case BLENDMODE_LIGHTEN:
			fogColor.xyz *= Luma(sourceColor);
			break;

		case BLENDMODE_SUBTRACTIVE:
		case BLENDMODE_EXCLUDE:
			fogColor.xyz *= 1.0f - Luma(sourceColor.xyz);
			break;

		case BLENDMODE_ALPHABLEND:
			fogColor.w = sourceColor.w;
			break;

		default:
			break;

	}

	if (fogColor.w > sourceColor.w)
		fogColor.w = sourceColor.w;

	float4 result = sourceColor;

	result.xyz += saturate(fogColor.xyz);

	return result;
}

float4 ApplyBlendModeColor(float4 sourceColor, float3 worldPosition, float4 positionCopy)
{
	if (BlendMode != BLENDMODE_DISTORTION)
		return sourceColor;

	float2 texCoord = 0.5f * (float2(positionCopy.x, -positionCopy.y) / positionCopy.w + 1.0f);
	float sceneDepth = DepthTexture.Sample(DepthSampler, saturate(texCoord)).x;
	float pixelDepth = positionCopy.z / positionCopy.w;

	sceneDepth = LinearizeDepth(sceneDepth, NearPlane, FarPlane);
	pixelDepth = LinearizeDepth(pixelDepth, NearPlane, FarPlane);

	if (pixelDepth - sceneDepth > 0.01f)
		discard;

	float mask = saturate(Luma(sourceColor.xyz) * sourceColor.w);
	float seed = frac(dot(worldPosition, float3(0.1031f, 0.11369f, 0.13787f)));
	return float4(mask, seed * mask, 0.0f, 0.0f);
}

#endif // BLENDINGSHADER
