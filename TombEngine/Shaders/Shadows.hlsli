#include "./Blending.hlsli"
#include "./Math.hlsli"
#include "./ShaderLight.hlsli"

#define SHADOW_INTENSITY (0.6f)
#define SHADOW_BLUR      (2.0f)

#define CSM_NUM_CASCADES 3
#define CSM_SHADOW_BLUR  5.0f
#define CSM_DEBUG_CASCADES 0  // Set to 1 to visualize cascades as colored bands

struct Sphere
{
    float3 position;
    float radius;
};

cbuffer ShadowLightBuffer : register(b4)
{
    ShaderLight Light;
    float4x4 LightViewProjections[6];
    int CastShadows;
    int NumSpheres;
    int ShadowMapSize;
    int padding;
    Sphere Spheres[16];
};

cbuffer SunLightBuffer : register(b9)
{
    float3 SunDirection;
    float SunIntensity;
    //--
    float3 SunColor;
    int SunEnabled;
    //--
    float4x4 CascadeViewProjections[CSM_NUM_CASCADES];
    //--
    // Packed as float4 to match C++ layout (float[3] would pad each element to 16 bytes).
    // x, y, z = cascade splits; w = shadow map pixel size.
    float4 CascadeSplitsAndPixelSize;
    //--
    float CSMDepthBias;
    float CSMSlopeBias;
    float CSMNormalBias;
    float CSMShadowIntensity;
};

Texture2DArray ShadowMap : register(t3);
SamplerComparisonState ShadowMapSampler : register(s3);

Texture2DArray CSMShadowMapTex : register(t14);
SamplerComparisonState CSMShadowMapSampler : register(s14);

float2 TexOffset(int u, int v) 
{
    return float2(u * 1.0f / ShadowMapSize, v * 1.0f / ShadowMapSize);
}


int GetCubeFaceIndex(float3 dir)
{
    float x = abs(dir.x);
    float y = abs(dir.y);
    float z = abs(dir.z);
    if (x > y && x > z)
        return 0 + (dir.x > 0 ? 0 : 1);
    else if (y > z)
        return 2 + (dir.y > 0 ? 0 : 1);
    return 4 + (dir.z > 0 ? 0 : 1);
}

float2 GetCubeUVFromDir(int faceIndex, float3 dir)
{
    float2 uv;
    switch (faceIndex)
    {
    case 0:
        uv = float2(-dir.z, dir.y);
        break; // +X
    case 1:
        uv = float2(dir.z, dir.y);
        break; // -X
    case 2:
        uv = float2(dir.x, dir.z);
        break; // +Y
    case 3:
        uv = float2(dir.x, -dir.z);
        break; // -Y
    case 4:
        uv = float2(dir.x, dir.y);
        break; // +Z
    default:
        uv = float2(-dir.x, dir.y);
        break; // -Z
    }
    return uv * .5 + .5;
}

float3 DoBlobShadows(float3 worldPos, float3 lighting)
{
    float shadowFactor = 1.0f;

    for (int i = 0; i < NumSpheres; i++)
    {
        Sphere s = Spheres[i];
        float dist = distance(worldPos, s.position);
        float insideSphere = saturate(1.0f - step(s.radius, dist));
        float radiusFactor = dist / s.radius;
        float factor = (1.0f - saturate(radiusFactor)) * insideSphere;
        shadowFactor -= factor * shadowFactor;
    }

    shadowFactor = saturate(shadowFactor);
    return lighting * saturate(1.0f - (1.0f - shadowFactor) * (SHADOW_INTENSITY * 0.5f));
}

float3 DoShadow(float3 worldPos, float3 normal, float3 lighting, float bias)
{
    if (!CastShadows)
        return lighting;

    if (BlendMode != BLENDMODE_OPAQUE && BlendMode != BLENDMODE_ALPHATEST && BlendMode != BLENDMODE_ALPHABLEND)	
        return lighting;

    float shadowFactor = 1.0f;

    float3 dir = normalize(Light.Position - worldPos);
    float ndot = dot(normal, dir);
    float facingFactor = saturate((ndot - bias) / (1.0f - bias + EPSILON));

    [unroll]
    for (int i = 0; i < 6; i++)
    {
        float4 lightClipSpace = mul(float4(worldPos, 1.0f), LightViewProjections[i]);
        lightClipSpace.xyz /= lightClipSpace.w;

        float insideLightBounds =
            step(-1.0f, lightClipSpace.x) * step(lightClipSpace.x, 1.0f) *
            step(-1.0f, lightClipSpace.y) * step(lightClipSpace.y, 1.0f) *
            step( 0.0f, lightClipSpace.z) * step(lightClipSpace.z, 1.0f);

        if (insideLightBounds > 0.0f)
        {
            lightClipSpace.x = lightClipSpace.x / 2 + 0.5;
            lightClipSpace.y = lightClipSpace.y / -2 + 0.5;

            float sum = 0;
            float samples = 0;

            // Perform basic PCF filtering
            for (float y = -SHADOW_BLUR; y <= SHADOW_BLUR; y += 1.0)
            {
                for (float x = -SHADOW_BLUR; x <= SHADOW_BLUR; x += 1.0)
                {
                    sum += ShadowMap.SampleCmpLevelZero(ShadowMapSampler, float3(lightClipSpace.xy + TexOffset(x, y), i), lightClipSpace.z);
                    samples += 1.0;
                }
            }

            shadowFactor = lerp(shadowFactor, sum / samples, facingFactor * insideLightBounds);
        }
    }

    float isPoint = step(0.5f, Light.Type == LT_POINT); // 1.0 if LT_POINT, 0.0 otherwise
    float isSpot  = step(0.5f, Light.Type == LT_SPOT);  // 1.0 if LT_SPOT,  0.0 otherwise
    float isOther = 1.0 - (isPoint + isSpot); // 1.0 if neither LT_POINT nor LT_SPOT

    float pointFactor = Luma(DoPointLight(worldPos, normal, Light));
    float spotFactor  = Luma(DoSpotLight(worldPos, normal, Light));

    float3 pointShadow = lighting * saturate(1.0f - (1.0f - shadowFactor) * SHADOW_INTENSITY * pointFactor);
    float3 spotShadow  = lighting * saturate(1.0f - (1.0f - shadowFactor) * SHADOW_INTENSITY * spotFactor );

    return pointShadow * isPoint + spotShadow * isSpot + lighting * isOther;
}

float3 DoSunShadow(float3 worldPos, float3 normal, float3 lighting)
{
    if (!SunEnabled)
        return lighting;

    if (BlendMode != BLENDMODE_OPAQUE && BlendMode != BLENDMODE_ALPHATEST && BlendMode != BLENDMODE_ALPHABLEND)
        return lighting;

    // Facing factor: same formula as DoShadow for point/spot lights.
    // DoShadow uses bias = -2.5, giving facingFactor = saturate((ndot+2.5)/3.5),
    // which means facingFactor is always > 0 for any real surface — side faces
    // and even back faces still receive cast shadows from other geometry.
    // We use the same bias here so sun shadows behave consistently.
    float ndotl = dot(normal, -SunDirection);
    float facingBias = -2.5f;
    float facingFactor = saturate((ndotl - facingBias) / (1.0f - facingBias + EPSILON));

    // Compute view-space depth for cascade selection (use original position).
    float4 viewPos = mul(float4(worldPos, 1.0f), View);
    float depth = -viewPos.z;

    // Select cascade.
    int cascadeIndex = CSM_NUM_CASCADES - 1;
    for (int i = 0; i < CSM_NUM_CASCADES; i++)
    {
        if (depth < CascadeSplitsAndPixelSize[i])
        {
            cascadeIndex = i;
            break;
        }
    }

    // Compute world-space shadow texel size for this cascade from the VP matrix.
    // The VP combines view rotation with ortho projection, so the X scale is
    // distributed across the first column; its length recovers 2/worldWidth.
    float4x4 cascadeVP = CascadeViewProjections[cascadeIndex];
    float orthoScale = length(float3(cascadeVP[0][0], cascadeVP[1][0], cascadeVP[2][0]));
    float texelWorldSize = 2.0f * CascadeSplitsAndPixelSize.w / orthoScale;

    // Normal offset bias: shift along the surface normal to prevent acne.
    // saturate(ndotl) clamps negative values to 0, giving maximum offset on
    // back faces (sinAngle = 1) and decreasing offset as the surface faces
    // the sun more directly (where acne is least severe).
    float clamped = saturate(ndotl);
    float sinAngle = sqrt(1.0f - clamped * clamped);
    float3 biasedWorldPos = worldPos + normal * texelWorldSize * CSMNormalBias * sinAngle;

    // Project normal-offset position into selected cascade's light space.
    float4 lightClipSpace = mul(float4(biasedWorldPos, 1.0f), CascadeViewProjections[cascadeIndex]);
    lightClipSpace.xyz /= lightClipSpace.w;

    // If outside the cascade's depth range, no shadow data exists — bail out.
    if (lightClipSpace.z < 0.0f || lightClipSpace.z > 1.0f)
        return lighting;

    // Clamp XY to valid UV range instead of bailing out entirely. The normal
    // offset bias can push a position slightly outside the cascade's XY bounds;
    // clamping prevents hard shadow drop-out seams at cascade edges.
    float2 shadowUV;
    shadowUV.x = saturate(lightClipSpace.x *  0.5f + 0.5f);
    shadowUV.y = saturate(lightClipSpace.y * -0.5f + 0.5f);
    float compareDepth = lightClipSpace.z;

    // Depth + slope bias. saturate(ndotl) caps the slope bias at CSMSlopeBias
    // for back faces instead of exceeding it (which caused cast shadows to vanish).
    float slopeBias = CSMSlopeBias * (1.0f - clamped);
    compareDepth -= CSMDepthBias + slopeBias;

    // PCF filtering (3x3).
    float sum = 0;
    float samples = 0;

    for (float y = -CSM_SHADOW_BLUR; y <= CSM_SHADOW_BLUR; y += 1.0)
    {
        for (float x = -CSM_SHADOW_BLUR; x <= CSM_SHADOW_BLUR; x += 1.0)
        {
            float2 offset = float2(x, y) * CascadeSplitsAndPixelSize.w;
            sum += CSMShadowMapTex.SampleCmpLevelZero(CSMShadowMapSampler, float3(shadowUV + offset, cascadeIndex), compareDepth);
            samples += 1.0;
        }
    }

    float shadowFactor = sum / samples;

    // Blend between cascades at boundaries.
    if (cascadeIndex < CSM_NUM_CASCADES - 1)
    {
        float splitDist = CascadeSplitsAndPixelSize[cascadeIndex];
        float blendRange = splitDist * 0.1f;
        float cascadeBlend = saturate((depth - (splitDist - blendRange)) / max(blendRange, EPSILON));

        if (cascadeBlend > 0.0f)
        {
            int nextCascade = cascadeIndex + 1;
            float4 nextClip = mul(float4(biasedWorldPos, 1.0f), CascadeViewProjections[nextCascade]);
            nextClip.xyz /= nextClip.w;

            float2 nextUV;
            nextUV.x = nextClip.x *  0.5f + 0.5f;
            nextUV.y = nextClip.y * -0.5f + 0.5f;
            float nextDepth = nextClip.z - CSMDepthBias - slopeBias;

            float nextSum = 0;
            float nextSamples = 0;
            for (float ny = -CSM_SHADOW_BLUR; ny <= CSM_SHADOW_BLUR; ny += 1.0)
            {
                for (float nx = -CSM_SHADOW_BLUR; nx <= CSM_SHADOW_BLUR; nx += 1.0)
                {
                    float2 nOffset = float2(nx, ny) * CascadeSplitsAndPixelSize.w;
                    nextSum += CSMShadowMapTex.SampleCmpLevelZero(CSMShadowMapSampler, float3(nextUV + nOffset, nextCascade), nextDepth);
                    nextSamples += 1.0;
                }
            }
            float nextShadowFactor = nextSum / nextSamples;
            shadowFactor = lerp(shadowFactor, nextShadowFactor, cascadeBlend);
        }
    }

    // Blend shadow result with facingFactor, same pattern as DoShadow:
    // facingFactor = 0 → shadowFactor stays 1.0 (no darkening, back-face).
    // facingFactor = 1 → shadowFactor is the full CSM result.
    shadowFactor = lerp(1.0f, shadowFactor, facingFactor);

    float intensity = min(CSMShadowIntensity, SHADOW_INTENSITY);
    float darken = lerp(1.0f - intensity, 1.0f, shadowFactor);

#if CSM_DEBUG_CASCADES
    // Debug: visualize cascade index as colored tint.
    float3 cascadeColors[3] = { float3(1,0.3,0.3), float3(0.3,1,0.3), float3(0.3,0.3,1) };
    return lighting * darken * cascadeColors[cascadeIndex];
#else
    return lighting * darken;
#endif
}