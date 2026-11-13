// God ray functions and pixel shaders, included by GodRay.hlsl. The including file declares the god ray
// uniforms, the textures, VSOutput and GOD_RAY_SAMPLER(texture), the sampler of a texture.

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// UV-space hash for per-pixel jitter (ported from the GLSL original).
float GodRayHash(float2 p)
{
    return frac(sin(dot(p, float2(41.0f, 289.0f))) * 45758.5453f);
}

// ---------------------------------------------------------------------------
// PSGodRay - radial light-shaft accumulation (half-res pass)
// ---------------------------------------------------------------------------

float4 PSGodRay(VSOutput input) : SV_TARGET
{
    float2 uv = input.UV;

    // Early out: sun behind the camera (sentinel -10) or automatic strength is zero.
    if (GodRaySunScreenPos.x < -5.0f || GodRayAutoStrength < 0.001f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float  aspect     = GodRayViewSize.x / GodRayViewSize.y;
    float2 uvCentered = (uv - 0.5f) * float2(aspect, 1.0f);

    float2 toSun     = uv - GodRaySunScreenPos;
    float2 marchStep = toSun * clamp(GodRayLength, 0.01f, 1.5f) / (float)GodRaySampleCount;
    float2 sampleUV  = uv + marchStep * GodRayHash(uv);

    float discScale = GodRaySoftness * 8.0f;

    // =========================================================================
    // UNDERWATER PATH - distinct from normal god rays.
    // The cloud RT is empty when UnderwaterSky is active. Instead, sample the
    // main scene RT (slot 3) which contains the rendered underwater sky. Only the
    // medium-bright glow rim (not the solid white core) acts as the shaft source
    // so rays appear to diverge from the edge of the sun glow, not its center.
    // =========================================================================
    if (GodRayUnderwaterActive > 0.5f)
    {
        // Dedicated march step using underwater-specific length.
        // When the sun is near the zenith (elevation -> 1.0), physically the
        // shafts should be very short because the light travels straight down
        // through the water column. Without this attenuation the rays extend
        // far across the screen, looking unnatural.
        float zenithAtten = 1.0f - smoothstep(0.55f, 0.95f, GodRaySunElevation);
        float lengthScale = lerp(0.22f, 1.0f, zenithAtten);

        float2 toSunUW     = uv - GodRaySunScreenPos;
        float2 marchStepUW = toSunUW * clamp(GodRayUnderwaterRayLength, 0.01f, 1.5f) * lengthScale / (float)GodRayUnderwaterSampleCount;
        float2 sampleUVUW  = uv + marchStepUW * GodRayHash(uv);

        float  accumulated = 0.0f;
        float  w           = 1.0f;

        [loop]
        for (int i = 0; i < GodRayUnderwaterSampleCount; i++)
        {
            sampleUVUW -= marchStepUW;
            float2 cuv = saturate(sampleUVUW);
            float sampleOnScreen = (sampleUVUW.x >= 0.0f && sampleUVUW.x <= 1.0f &&
                                    sampleUVUW.y >= 0.0f && sampleUVUW.y <= 1.0f) ? 1.0f : 0.0f;

            float3 sceneRgb  = SceneTexture.SampleLevel(GOD_RAY_SAMPLER(SceneTexture), cuv, 0).rgb;
            float  sceneLuma = dot(saturate(sceneRgb), float3(0.299f, 0.587f, 0.114f));

            // Natural luminance source: scene brightness drives ray emission.
            // Sharpness controls contrast - higher values suppress dim pixels more.
            float source = pow(sceneLuma, max(0.5f, GodRayUnderwaterSharpness));
            source = saturate(source * GodRayUnderwaterBrightness);

            accumulated += source * sampleOnScreen * w;
            w *= GodRayUnderwaterRayDecay;
        }

        accumulated /= (float)GodRayUnderwaterSampleCount;
        float3 col = GodRaySunColor * accumulated * GodRayUnderwaterRayIntensity * GodRayAutoStrength;
        col *= smoothstep(1.2f, 0.6f, length(uvCentered));
        return float4(sqrt(saturate(col)), 0.0f);
    }

    // =========================================================================
    // NORMAL PATH - above water.
    // Uses cloud RT occlusion mask + virtual sun disc. The sun disc fades as
    // clouds or the horizon cover the sun, more aggressively at sunset.
    // =========================================================================

    float2 sunEdgeDist    = max(abs(GodRaySunScreenPos - 0.5f) - 0.5f, 0.0f);
    float  sunOnScreenFade = 1.0f - saturate(max(sunEdgeDist.x, sunEdgeDist.y) * 6.0f);
    float2 sunUV           = saturate(GodRaySunScreenPos);
    float4 sunCloudSampleA = CloudTexture.SampleLevel(GOD_RAY_SAMPLER(CloudTexture), sunUV, 0);
    float4 sunCloudSampleB = CloudTextureB.SampleLevel(GOD_RAY_SAMPLER(CloudTextureB), sunUV, 0);
    float  sunCloudA       = sunCloudSampleA.a;
    float  sunCloudB       = sunCloudSampleB.a;
    float  sunHorizonMask  = HorizonMaskTexture.SampleLevel(GOD_RAY_SAMPLER(HorizonMaskTexture), sunUV, 0).r;
    float  sunOcclusion    = saturate(max(sunCloudA, sunCloudB) + sunHorizonMask);
    float3 sunCloudRGB        = (sunCloudA >= sunCloudB) ? sunCloudSampleA.rgb : sunCloudSampleB.rgb;
    float  sunCloudLuma       = dot(saturate(sunCloudRGB), float3(0.299f, 0.587f, 0.114f));
    float  cloudPresenceGate  = saturate(sunOcclusion * 20.0f);
    float  darkUndersideBoost = saturate(1.0f - sunCloudLuma / 0.5f) * cloudPresenceGate;
    float sunsetFac       = saturate(1.0f - GodRaySunElevation / 0.25f);
    float godRayOccAggr   = lerp(1.0f, 8.0f, max(sunsetFac, darkUndersideBoost));
    float sunOccludeInput = saturate(sunOcclusion * godRayOccAggr);
    float  occludeT   = smoothstep(0.2f, 0.55f, sunOccludeInput);
    occludeT          = occludeT * occludeT * occludeT;
    float  sunDiscVis = sunOnScreenFade * (1.0f - occludeT);

    float  accumulated = 0.0f;
    float  w           = 1.0f;

    [loop]
    for (int i = 0; i < GodRaySampleCount; i++)
    {
        sampleUV -= marchStep;
        float2 cuv = saturate(sampleUV);
        float sampleOnScreen = (sampleUV.x >= 0.0f && sampleUV.x <= 1.0f &&
                                sampleUV.y >= 0.0f && sampleUV.y <= 1.0f) ? 1.0f : 0.0f;

        float cloudAlphaA  = CloudTexture.SampleLevel(GOD_RAY_SAMPLER(CloudTexture), cuv, 0).a;
        float cloudAlphaB  = CloudTextureB.SampleLevel(GOD_RAY_SAMPLER(CloudTextureB), cuv, 0).a;
        float horizonMask  = HorizonMaskTexture.SampleLevel(GOD_RAY_SAMPLER(HorizonMaskTexture), cuv, 0).r;
        float cloudOpacity = saturate(max(cloudAlphaA, cloudAlphaB) + horizonMask);
        float cloudGap     = smoothstep(0.05f, 0.35f, 1.0f - cloudOpacity);

        float sunDist = distance(cuv, GodRaySunScreenPos);
        float sunDisc = exp(-sunDist * discScale) * sunDiscVis;

        float sceneBrightness = saturate(cloudGap + sunDisc) * sampleOnScreen;
        accumulated += sceneBrightness * w;
        w *= GodRayDecay;
    }

    accumulated /= (float)GodRaySampleCount;
    float3 col = GodRaySunColor * accumulated * GodRayIntensity * GodRayAutoStrength;
    col *= smoothstep(1.2f, 0.6f, length(uvCentered));
    return float4(sqrt(saturate(col)), 0.0f);
}

// ---------------------------------------------------------------------------
// PSGodRayComposite - blend half-res god rays onto the full-res scene
// ---------------------------------------------------------------------------
// Uses BlendMode::Additive on the C++ side (source alpha + one, see GetBlendState in SceneRenderer.cpp).
// Texture slot 0 is rebound to the half-res god ray render target before this pass.

float4 PSGodRayComposite(VSOutput input) : SV_TARGET
{
    float3 godRay = CloudTexture.Sample(GOD_RAY_SAMPLER(CloudTexture), input.UV).rgb;
    // alpha=1 is required: BlendMode::Additive uses the source alpha as source blend factor.
    // With alpha=0 the entire contribution would be multiplied to zero and nothing would appear.
    return float4(godRay, 1.0f);
}
