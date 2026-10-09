#ifndef TEN_DECALS
#define TEN_DECALS

// Room decals of the legacy renderer (Shaders/Rooms.hlsl): noisy blood stains darkening the lighting around
// each decal center. Decals of the visible rooms live in a frame storage buffer; rooms draw a slice of it.

#include "./ShaderInterop.hlsli"

struct GpuDecal
{
    float3 Position;
    float  Radius;
    float  Opacity;
    int    Pattern;
    float2 Padding;
};

TEN_PS_STORAGE_BUFFER(GpuDecal, Decals, 9);

float2 GetGradient2D(float2 p)
{
    float angle = 6.28318530718f * frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
    return float2(cos(angle), sin(angle));
}

// Single octave gradient noise, as the legacy NebularNoise with its fixed parameters.
float GetDecalNoise(float2 uv)
{
    float2 cell = floor(uv);
    float2 local = frac(uv);

    float dot1 = dot(GetGradient2D(cell), local);
    float dot2 = dot(GetGradient2D(cell + float2(1.0f, 0.0f)), local - float2(1.0f, 0.0f));
    float dot3 = dot(GetGradient2D(cell + float2(0.0f, 1.0f)), local - float2(0.0f, 1.0f));
    float dot4 = dot(GetGradient2D(cell + float2(1.0f, 1.0f)), local - float2(1.0f, 1.0f));

    float2 a = lerp(float2(dot1, dot3), float2(dot2, dot4), smoothstep(0.0f, 1.0f, local.x));
    return lerp(a.x, a.y, smoothstep(0.0f, 1.0f, local.y));
}

float GetDecalMask(float3 position, float3 tangent, float3 binormal, uint decalOffset, uint decalCount)
{
    float mask = 0.0f;

    for (uint i = 0; i < decalCount; i++)
    {
        GpuDecal decal = Decals[decalOffset + i];

        float3 offset = position - decal.Position;
        float distanceToCenter = length(offset);
        if (distanceToCenter > decal.Radius * 1.3f)
            continue;

        float2 uv = float2(dot(offset, tangent), dot(offset, binormal)) * (8.0f / (decal.Pattern + 1) * 2.0f / decal.Radius);
        float noisyRadius = decal.Radius * (1.0f + 0.25f * (GetDecalNoise(uv) * 2.0f - 1.0f));
        float holeRadius = decal.Radius / 4.0f;

        float edge = saturate((noisyRadius - distanceToCenter) / noisyRadius);
        float fade = saturate((decal.Radius - distanceToCenter) / decal.Radius);
        float hole = saturate((holeRadius - distanceToCenter) / (holeRadius * 1.3f)) * (1 - decal.Pattern);

        mask = max(mask, (edge * fade + hole) * decal.Opacity);
    }

    return mask;
}

#endif // TEN_DECALS
