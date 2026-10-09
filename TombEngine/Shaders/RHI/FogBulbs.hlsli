#ifndef TEN_FOG_BULBS
#define TEN_FOG_BULBS

// Fog bulbs of the legacy renderer (Shaders/ShaderLight.hlsli): fog spheres evaluated per vertex from the length
// of the camera ray inside each sphere. xyz is the accumulated fog color, w the accumulated density.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"

TEN_VS_UNIFORMS(FogBulbUniforms, 3)
{
    TEN_FOG_BULB_UNIFORMS
};

// Length of the camera ray segment inside the bulb; isSky treats the position as always outside the bulb.
float GetFogBulbDensity(float3 position, float3 cameraPosition, GpuFogBulb bulb, bool isSky)
{
    float3 direction = normalize(position - cameraPosition);
    float3 bulbToVertex = position - bulb.Position;
    bool isVertexInside = !isSky && (dot(bulbToVertex, bulbToVertex) < bulb.SquaredRadius);
    bool isCameraInside = (bulb.SquaredCameraToFogBulbDistance < bulb.SquaredRadius);

    float tca = dot(bulb.FogBulbToCameraVector, direction);
    float d2 = bulb.SquaredCameraToFogBulbDistance - tca * tca;

    float3 p0 = cameraPosition;
    float3 p1 = position;

    if (isCameraInside)
    {
        if (!isVertexInside)
            p1 = cameraPosition + direction * (tca + sqrt(bulb.SquaredRadius - d2));
    }
    else if (isVertexInside)
    {
        p0 = cameraPosition + direction * (tca - sqrt(bulb.SquaredRadius - d2));
    }
    else
    {
        float3 cameraToVertex = position - cameraPosition;
        bool isAhead = (tca > 0.0f) && (isSky || dot(cameraToVertex, cameraToVertex) > tca * tca);
        if (!isAhead || d2 >= bulb.SquaredRadius)
            return 0.0f;

        float thc = sqrt(bulb.SquaredRadius - d2);
        p0 = cameraPosition + direction * (tca - thc);
        p1 = cameraPosition + direction * (tca + thc);
    }

    return length(p1 - p0) * bulb.Density / 255.0f;
}

float4 GetFogBulbs(float3 position, float3 cameraPosition, bool isSky)
{
    float4 fog = float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < min(FogBulbCount, FOG_BULBS_MAX); i++)
    {
        float density = GetFogBulbDensity(position, cameraPosition, FogBulbs[i], isSky);
        fog.xyz += FogBulbs[i].Color * COLOR_MODULATION_SCALE * density;
        fog.w = saturate(fog.w + density);
    }

    return fog;
}

// Legacy DoFogBulbsForPixel: adds the bulb color, weighted by the blend mode like the distance fog.
float4 ApplyFogBulbs(float4 color, float3 fogColor, uint blendMode)
{
    if (blendMode == BLENDMODE_ADDITIVE || blendMode == BLENDMODE_SCREEN || blendMode == BLENDMODE_LIGHTEN)
        fogColor *= Luma(color.rgb);
    else if (blendMode == BLENDMODE_SUBTRACTIVE || blendMode == BLENDMODE_EXCLUDE)
        fogColor *= 1.0f - Luma(color.rgb);

    return float4(color.rgb + saturate(fogColor), color.a);
}

#endif // TEN_FOG_BULBS
