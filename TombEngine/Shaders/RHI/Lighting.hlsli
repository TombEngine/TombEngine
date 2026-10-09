#ifndef TEN_LIGHTING
#define TEN_LIGHTING

// Light model of the legacy renderer (Shaders/ShaderLight.hlsli), ported unchanged.
// Light lists are slices of the frame light buffer: rooms pass offset and count per room,
// object instances store them in their instance data.

#include "./Common.hlsli"

#define LT_SUN    0
#define LT_POINT  1
#define LT_SPOT   2
#define LT_SHADOW 3

#define LT_MASK        0xFFFF
#define LIGHTS_PER_DRAW_MAX 256 // Loop bound: a corrupted count must not hang the GPU (Vulkan loses the device).
#define LT_MASK_SUN    (1 << (31 - LT_SUN))
#define LT_MASK_POINT  (1 << (31 - LT_POINT))
#define LT_MASK_SPOT   (1 << (31 - LT_SPOT))
#define LT_MASK_SHADOW (1 << (31 - LT_SHADOW))

#define ROOM_LIGHT_COEFF 0.7f
#define SPEC_FACTOR      64.0f

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

// Specular terms. A positive strength (vertex sheen) overrides the material specular intensity and
// roughness. The legacy model uses the camera direction rather than the per-pixel view vector.

float RoughnessToExponentMultiplier(float roughness)
{
    float gloss = 1.0f - max(saturate(roughness), 0.04f);
    return lerp(0.04f, 4.0f, gloss * gloss);
}

float GetSpecularTerm(float3 lightDirection, float3 normal, float3 cameraDirection, float strength, float roughness)
{
    float m = saturate(sign(strength));
    float exponentMultiplier = lerp(RoughnessToExponentMultiplier(roughness), 1.0f, m);
    float exponent = max(lerp(1.0f, strength, m) * SPEC_FACTOR * exponentMultiplier, 1.0f);
    return pow(saturate(dot(cameraDirection, reflect(lightDirection, normal))), exponent);
}

float GetSpecularIntensity(GpuLight light, float strength, float specular)
{
    return lerp(saturate(specular), 1.0f, saturate(sign(strength))) * light.Intensity;
}

float3 DoSpecularPoint(float3 position, float3 normal, float3 cameraDirection, GpuLight light, float strength, float specular, float roughness)
{
    float attenuation = saturate((light.Out - distance(light.Position, position)) / max(light.Out, 0.001f));
    float3 lightDirection = normalize(light.Position - position);
    return attenuation * GetSpecularTerm(lightDirection, normal, cameraDirection, strength, roughness) * light.Color * GetSpecularIntensity(light, strength, specular);
}

float3 DoSpecularSun(float3 normal, float3 cameraDirection, GpuLight light, float strength, float specular, float roughness)
{
    float3 lightDirection = -normalize(light.Direction);
    return GetSpecularTerm(lightDirection, normal, cameraDirection, strength, roughness) * light.Color * GetSpecularIntensity(light, strength, specular);
}

float3 DoSpecularSpot(float3 position, float3 normal, float3 cameraDirection, GpuLight light, float strength, float specular, float roughness)
{
    float lightDistance = distance(light.Position, position);
    float3 lightDirection = normalize(light.Position - position);

    float cosine = dot(-lightDirection, light.Direction);
    float angleAttenuation = saturate((cosine - light.OutRange) / (light.InRange - light.OutRange));
    float distanceAttenuation = saturate((light.Out - lightDistance) / (light.Out - light.In));

    return angleAttenuation * distanceAttenuation * GetSpecularTerm(lightDirection, normal, cameraDirection, strength, roughness) *
           light.Color * GetSpecularIntensity(light, strength, specular);
}

#endif // TEN_LIGHTING
