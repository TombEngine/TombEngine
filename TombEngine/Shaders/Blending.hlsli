#ifndef BLENDINGSHADER
#define BLENDINGSHADER

#include "./CBCamera.hlsli"
#include "./CBPerDraw.hlsli"
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

#define DISTORTION_DEPTH_SCALE 10240.0f

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

// Encodes a distortion payload for additive RGBA8_Unorm accumulation.
// surfaceNormal: actual surface normal for geometry, or camera-facing direction for sprites.
// Split signed direction into positive/negative channels:
//   R = max(0, dir.x), G = max(0, dir.y), B = max(0, -dir.x), A = max(0, -dir.y); all * strength.
//   Decoder: netDir = float2(R - B, G - A); weight = length(netDir).
inline float4 EncodeDistortionPayload(float4 sourceColor, float3 surfaceNormal, float4 positionCopy)
{
	float luma = saturate(Luma(sourceColor.xyz));
	float strength = saturate(luma * sourceColor.w);

	if (strength <= EPSILON)
		return 0.0f;

	float3 normalWS = SafeNormalize(surfaceNormal);
	float3 normalVS = SafeNormalize(mul(float4(normalWS, 0.0f), View).xyz);

	float2 gradient = float2(ddx(luma), ddy(luma));
	float2 gradientDir = SafeNormalize(float3(gradient, 0.0f)).xy;
	float2 projectedNormalDir = SafeNormalize(float3(normalVS.xy, 0.0f)).xy;

	float orientationFactor = lerp(0.35f, 1.0f, saturate(length(normalVS.xy)));

	float2 refractDir = projectedNormalDir;
	if (dot(gradientDir, gradientDir) > EPSILON)
		refractDir = SafeNormalize(float3(gradientDir + projectedNormalDir, 0.0f)).xy;

	if (dot(refractDir, refractDir) <= EPSILON)
		return 0.0f;

	strength *= orientationFactor;

	return float4(
		max(0.0f,  refractDir.x) * strength,
		max(0.0f,  refractDir.y) * strength,
		max(0.0f, -refractDir.x) * strength,
		max(0.0f, -refractDir.y) * strength);
}

float4 ApplyBlendModeColor(float4 sourceColor, float3 worldPosition, float3 surfaceNormal, float4 positionCopy)
{
	if (BlendMode != BLENDMODE_DISTORTION)
		return sourceColor;

	return EncodeDistortionPayload(sourceColor, surfaceNormal, positionCopy);
}

float4 ApplyBlendModeColor(float4 sourceColor, float3 worldPosition, float4 positionCopy)
{
	if (BlendMode != BLENDMODE_DISTORTION)
		return sourceColor;

	float3 cameraFacingNormal = SafeNormalize(CamPositionWS.xyz - worldPosition);
	return EncodeDistortionPayload(sourceColor, cameraFacingNormal, positionCopy);
}

#endif // BLENDINGSHADER
