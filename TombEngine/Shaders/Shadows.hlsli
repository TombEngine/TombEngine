#ifndef TEN_SHADOWS
#define TEN_SHADOWS

// Shadows of the frontend (Shaders/Shadows.hlsli): a cube shadow map for the shadow casting light, stored as
// six R32 float layers (SDL GPU has no depth array targets), compared in the shader with 5x5 PCF, and blob shadows
// from up to 16 spheres around the player and creatures.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./Lighting.hlsli"

#define SHADOW_INTENSITY 0.6f
#define SHADOW_BLUR      2.0f

// Instances receiving shadows have this bit in their light count.
#define SHADOWABLE_MASK (1 << 16)

TEN_PS_UNIFORMS(ShadowUniforms, 2)
{
    TEN_SHADOW_UNIFORMS
};

TEN_PS_TEXTURE(Texture2DArray, ShadowMap, 5);

float3 DoBlobShadows(float3 position, float3 lighting)
{
    float shadowFactor = 1.0f;

    for (int i = 0; i < min(SphereCount, SHADOW_SPHERES_MAX); i++)
    {
        float distanceToCenter = distance(position, Spheres[i].xyz);
        float inside = 1.0f - step(Spheres[i].w, distanceToCenter);
        float factor = (1.0f - saturate(distanceToCenter / Spheres[i].w)) * inside;
        shadowFactor -= factor * shadowFactor;
    }

    return lighting * saturate(1.0f - (1.0f - saturate(shadowFactor)) * (SHADOW_INTENSITY * 0.5f));
}

float3 DoShadow(float3 position, float3 normal, float3 lighting, float bias, uint blendMode)
{
    if (!CastShadows)
        return lighting;

    if (blendMode != BLENDMODE_OPAQUE && blendMode != BLENDMODE_ALPHATEST && blendMode != BLENDMODE_ALPHABLEND)
        return lighting;

    float shadowFactor = 1.0f;

    float3 lightDirection = normalize(ShadowLight.Position - position);
    float facingFactor = saturate((dot(normal, lightDirection) - bias) / (1.0f - bias + 0.001f));
    float texel = 1.0f / ShadowMapSize;

    [unroll]
    for (int face = 0; face < 6; face++)
    {
        float4 lightClip = mul(float4(position, 1.0f), LightViewProjections[face]);
        lightClip.xyz /= lightClip.w;

        float inside =
            step(-1.0f, lightClip.x) * step(lightClip.x, 1.0f) *
            step(-1.0f, lightClip.y) * step(lightClip.y, 1.0f) *
            step(0.0f, lightClip.z) * step(lightClip.z, 1.0f);

        if (inside > 0.0f)
        {
            float2 uv = float2(lightClip.x * 0.5f + 0.5f, lightClip.y * -0.5f + 0.5f);

            float sum = 0.0f;
            float sampleCount = 0.0f;
            for (float y = -SHADOW_BLUR; y <= SHADOW_BLUR; y += 1.0f)
            {
                for (float x = -SHADOW_BLUR; x <= SHADOW_BLUR; x += 1.0f)
                {
                    float storedDepth = ShadowMap.SampleLevel(ShadowMapSampler, float3(uv + float2(x, y) * texel, face), 0.0f).x;
                    sum += (lightClip.z <= storedDepth) ? 1.0f : 0.0f;
                    sampleCount += 1.0f;
                }
            }

            shadowFactor = lerp(shadowFactor, sum / sampleCount, facingFactor * inside);
        }
    }

    float lightFactor = 0.0f;
    if (ShadowLight.Type == LT_POINT)
        lightFactor = Luma(DoPointLight(position, normal, ShadowLight));
    else if (ShadowLight.Type == LT_SPOT)
        lightFactor = Luma(DoSpotLight(position, normal, ShadowLight));
    else
        return lighting;

    return lighting * saturate(1.0f - (1.0f - shadowFactor) * SHADOW_INTENSITY * lightFactor);
}

#endif // TEN_SHADOWS
