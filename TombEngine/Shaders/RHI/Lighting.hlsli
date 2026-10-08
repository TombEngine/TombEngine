#ifndef TEN_LIGHTING
#define TEN_LIGHTING

// Diffuse light model of the legacy renderer (Shaders/ShaderLight.hlsli), ported unchanged.
// Light lists are slices of the frame light buffer: rooms pass offset and count per room,
// object instances store them in their instance data.

#include "./Common.hlsli"

#define LT_SUN    0
#define LT_POINT  1
#define LT_SPOT   2
#define LT_SHADOW 3

#define LT_MASK        0xFFFF
#define LT_MASK_SUN    (1 << (31 - LT_SUN))
#define LT_MASK_POINT  (1 << (31 - LT_POINT))
#define LT_MASK_SPOT   (1 << (31 - LT_SPOT))
#define LT_MASK_SHADOW (1 << (31 - LT_SHADOW))

#define ROOM_LIGHT_COEFF 0.7f

// Same layout as ShaderLight in the legacy renderer; spot ranges are stored as cosines.
struct GpuLight
{
    float3 Position;
    uint   Type;
    float3 Color;
    float  Intensity;
    float3 Direction;
    float  In;
    float  Out;
    float  InRange;
    float  OutRange;
    float  Padding;
};

float3 DoPointLight(float3 position, float3 normal, GpuLight light)
{
    float3 lightVector = light.Position - position;
    float  distance = length(lightVector);
    float3 lightDirection = normalize(lightVector);

    float attenuation = saturate((light.Out - distance) / (light.Out - light.In));
    float d = saturate(dot(normal, lightDirection));

    return saturate(light.Color * light.Intensity * attenuation * d);
}

float3 DoShadowLight(float3 position, float3 normal, GpuLight light)
{
    float3 lightVector = light.Position - position;
    float distance = length(lightVector);
    float3 lightDirection = normalize(lightVector);

    float attenuation = saturate((light.Out - distance) / (light.Out - light.In));
    float d = saturate(dot(normal, lightDirection));

    float absolute = light.Color.x * light.Intensity * attenuation;
    float directional = absolute * d;

    return saturate((absolute * 0.33f) + (directional * 0.66f)) * 2.0f;
}

float3 DoSpotLight(float3 position, float3 normal, GpuLight light)
{
    float3 lightVector = position - light.Position;
    float  distance = length(lightVector);
    float3 lightDirection = normalize(lightVector);

    float cosine = dot(lightDirection, light.Direction);
    float angleAttenuation = saturate((cosine - light.OutRange) / (light.InRange - light.OutRange));
    float distanceAttenuation = saturate((light.Out - distance) / (light.Out - light.In));

    float d = saturate(dot(normal, -lightDirection));
    return saturate(light.Color * light.Intensity * angleAttenuation * distanceAttenuation * d);
}

float3 DoDirectionalLight(float3 normal, GpuLight light)
{
    float d = saturate(dot(-light.Direction, normal));
    return light.Color * light.Intensity * d;
}

#endif // TEN_LIGHTING
