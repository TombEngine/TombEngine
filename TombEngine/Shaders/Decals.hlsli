#ifndef TEN_DECALS
#define TEN_DECALS

// Room decals, used by Rooms.hlsl: noisy blood stains darkening the lighting around
// each decal center. Decals of the visible rooms live in a frame storage buffer; rooms draw a slice of it.

#include "./GpuShared.hlsli"
#include "./ShaderInterop.hlsli"
#include "./Noise.hlsli"

TEN_PS_RAW_BUFFER(GpuDecal, Decals, 11);

#define DECALS_PER_DRAW_MAX 64

// Strongest decal coverage in [0, 1] at a surface point: a noise-perturbed disc in the surface tangent plane,
// plus a solid core for pattern 0.
float GetDecalMask(float3 position, float3 tangent, float3 binormal, uint decalOffset, uint decalCount)
{
    float mask = 0.0f;

    for (uint i = 0; i < min(decalCount, DECALS_PER_DRAW_MAX); i++)
    {
        GpuDecal decal = TEN_LOAD_RAW(GpuDecal, Decals, decalOffset + i);

        float3 offset = position - decal.Position;
        float distanceToCenter = length(offset);
        if (distanceToCenter > decal.Radius * 1.3f)
            continue;

        float2 uv = float2(dot(offset, tangent), dot(offset, binormal)) * (8.0f / (decal.Pattern + 1) * 2.0f / decal.Radius);
        float noisyRadius = decal.Radius * (1.0f + 0.25f * (NebularNoise(uv, 1.0f, 0.5f, 0.3f) * 2.0f - 1.0f));
        float holeRadius = decal.Radius / 4.0f;

        float edge = saturate((noisyRadius - distanceToCenter) / noisyRadius);
        float fade = saturate((decal.Radius - distanceToCenter) / decal.Radius);
        float hole = saturate((holeRadius - distanceToCenter) / (holeRadius * 1.3f)) * (1 - decal.Pattern);

        mask = max(mask, (edge * fade + hole) * decal.Opacity);
    }

    return mask;
}

#endif // TEN_DECALS
