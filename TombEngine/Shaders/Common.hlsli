#ifndef TEN_COMMON
#define TEN_COMMON

// Shared constants, vertex data decoding, animated textures and fog for the RHI shaders.

#include "./GpuShared.hlsli"

#define PI                     3.14159265f
#define COLOR_MODULATION_SCALE 2.0f

#define BLENDMODE_OPAQUE     0
#define BLENDMODE_ALPHATEST  1
#define BLENDMODE_ADDITIVE   2
#define BLENDMODE_DISTORTION 3
#define BLENDMODE_SUBTRACTIVE 5
#define BLENDMODE_EXCLUDE    8
#define BLENDMODE_SCREEN     9
#define BLENDMODE_LIGHTEN    10
#define BLENDMODE_ALPHABLEND 11

#define ANIM_TYPE_FRAMES   0
#define ANIM_TYPE_UVROTATE 1
#define ANIM_TYPE_VIDEO    2
#define ANIM_TYPE_LEGACY_WATERFALL 100 // Waterfall objects, see SceneRenderer::AddWaterfallSets.

#define LIGHT_MODE_DYNAMIC 0
#define LIGHT_MODE_STATIC  1

float LinearizeDepth(float depth, float nearPlane, float farPlane)
{
    return (nearPlane * 2.0f) / (farPlane + nearPlane - depth * (farPlane - nearPlane));
}

float Luma(float3 color)
{
    return (color.x * 0.2126f) + (color.y * 0.7152f) + (color.z * 0.0722f);
}

uint DecodeIndexInPoly(uint effects)
{
    return (effects >> 25) & 0x3;
}

uint DecodeAnimationFrameOffset(uint animationData)
{
    return (animationData >> 24) & 0xFF;
}

// Vertex effects.

#define WIBBLE_FRAME_PERIOD 64.0f

float DecodeGlow(uint effects)
{
    return (float)(effects & 0xFF) / 255.0f;
}

float DecodeMove(uint effects)
{
    return (float)((effects >> 8) & 0xFF) / 255.0f;
}

float DecodeSheen(uint effects)
{
    return (float)((effects >> 16) & 0xFF) / 255.0f;
}

float DecodeWeight(uint effects)
{
    return (float)((effects >> 24) & 0x1);
}

uint DecodeHash(uint animationData)
{
    return animationData & 0xFF;
}

float Wibble(uint effects, uint hash, float interpolatedFrame)
{
    float enabled = ((DecodeGlow(effects) + DecodeMove(effects)) > 0.0f) ? 1.0f : 0.0f;
    float phase = frac(interpolatedFrame / WIBBLE_FRAME_PERIOD + (float)hash / 256.0f);
    return sin(phase * 6.28318530718f) * enabled;
}

float3 Glow(float3 color, uint effects, float wibble)
{
    float intensity = DecodeGlow(effects) * lerp(-0.5f, 1.0f, wibble * 0.5f + 0.5f) * 0.5f;
    return color + intensity;
}

float3 Move(float3 position, uint effects, float wibble)
{
    float offset = wibble * DecodeMove(effects) * DecodeWeight(effects) * 128.0f;
    return position + float3(0.0f, offset, 0.0f);
}

uint GetAnimatedFrameIndex(GpuAnimatedSet set, uint frame, uint frameOffset)
{
    float speed = (float)set.Fps / 30.0f;
    return set.FirstFrame + ((int)(frame * speed + frameOffset) % max(set.NumFrames, 1));
}

float2 GetFrameCorner(GpuAnimatedFrame frame, uint indexInPoly)
{
    switch (indexInPoly)
    {
    case 0:
        return frame.TopLeft;

    case 1:
        return frame.TopRight;

    case 2:
        return frame.BottomRight;

    default:
        return frame.BottomLeft;
    }
}

// UV rotation scrolls the texture over time inside the frame bounds. The offset is constant per draw,
// so it is computed per vertex and applied with frac() per pixel.
float3 GetUVRotateScroll(GpuAnimatedFrame frame, GpuAnimatedSet set, float interpolatedFrame)
{
    if (set.UVRotateSpeed <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);

    float2 minUV = min(min(frame.TopLeft, frame.TopRight), min(frame.BottomLeft, frame.BottomRight));
    float2 maxUV = max(max(frame.TopLeft, frame.TopRight), max(frame.BottomLeft, frame.BottomRight));
    float2 uvSize = maxUV - minUV;

    float relativePosition = interpolatedFrame * set.UVRotateSpeed / 30.0f;
    float theta = radians(set.UVRotateDirection + 90.0f);
    float2 direction = float2(cos(theta), sin(theta));

    return float3(-direction * relativePosition * uvSize, 1.0f);
}

float2 ApplyUVScroll(float2 uv, float3 scroll)
{
    const float EPSILON = 0.001f;

    if (scroll.z < 0.5f)
        return uv;

    return clamp(frac(uv + scroll.xy), EPSILON, 1.0f - EPSILON);
}

float GetDistanceFog(float3 position, float3 cameraPosition, float fogMinDistance, float fogMaxDistance)
{
    if (fogMaxDistance <= 0.0f)
        return 0.0f;

    float distance = length(cameraPosition - position);
    return saturate((distance - fogMinDistance * 1024.0f) / (fogMaxDistance * 1024.0f - fogMinDistance * 1024.0f));
}

float4 ApplyDistanceFog(float4 color, float4 fogColor, float fog, uint blendMode)
{
    switch (blendMode)
    {
    case BLENDMODE_ADDITIVE:
    case BLENDMODE_SCREEN:
    case BLENDMODE_LIGHTEN:
        fogColor.xyz *= Luma(color.xyz);
        break;

    case BLENDMODE_SUBTRACTIVE:
    case BLENDMODE_EXCLUDE:
        fogColor.xyz *= 1.0f - Luma(color.xyz);
        break;

    case BLENDMODE_ALPHABLEND:
        fogColor.w = color.w;
        break;
    }

    fogColor.w = min(fogColor.w, color.w);
    return lerp(color, fogColor, fog);
}

// Distortion payload: strength, view depth split in two bytes and emitter type (0 billboard, 1 surface),
// all premultiplied by strength for additive accumulation.
float4 EncodeDistortionPayload(float4 color, float viewDepth, float emitterType)
{
    float strength = saturate(saturate(Luma(color.rgb)) * color.a);
    if (strength <= 1e-6f)
        return 0.0f;

    float depth = floor(clamp(viewDepth, 0.0f, 65535.0f));
    return float4(strength, floor(depth / 256.0f) / 255.0f * strength, fmod(depth, 256.0f) / 255.0f * strength, emitterType * strength);
}

#endif // TEN_COMMON
