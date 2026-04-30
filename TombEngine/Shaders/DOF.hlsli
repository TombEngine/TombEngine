// Depth-of-field: structs, downsample+CoC, CoC dilation, bokeh blur, composite.
// Should be included near the top of PostProcess.hlsl, after CB and texture declarations.
//
// DofParams layout (float4):
//   x = focus distance (view-space units)
//   y = focus range    (view-space units)
//   z = bokeh strength (max radius in half-res pixels at CoC = 1)
//   w = DOFMode:  0 = None, 1 = Full, 2 = Front, 3 = Back

// ---------------------------------------------------------------------------
// Shared input structs (used by VS and all DOF/postprocess PS functions).
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// Constants and kernel.
// ---------------------------------------------------------------------------

#define DOF_COC_EPSILON  0.025f
#define DOF_DISC_SAMPLES 12

static const float2 DOF_DISC_OFFSETS[DOF_DISC_SAMPLES] =
{
    float2(-0.3262f, -0.4058f),
    float2(-0.8401f, -0.0736f),
    float2(-0.6959f,  0.4571f),
    float2(-0.2033f,  0.6207f),
    float2( 0.9623f, -0.1950f),
    float2( 0.4734f, -0.4800f),
    float2( 0.5195f,  0.7670f),
    float2( 0.1855f, -0.8931f),
    float2( 0.5074f,  0.0644f),
    float2( 0.8964f,  0.4125f),
    float2(-0.3219f, -0.9326f),
    float2(-0.7916f, -0.5977f),
};

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------

// Returns a [0,1] circle-of-confusion for a given view-space depth.
float GetDOFCoC(float viewDepth)
{
    float signedCoC = (viewDepth - DofParams.x) / max(DofParams.y, 1.0f);
    int mode = (int)DofParams.w;
	
    if (mode == 1) // Full: blur on both sides.
        return saturate(abs(signedCoC));
    if (mode == 2) // Front: blur only pixels closer than focus.
        return saturate(-signedCoC);
    if (mode == 3) // Back: blur only pixels beyond focus.
		return saturate(signedCoC);
		
	return 0; // Other values: no DOF.
}

// ---------------------------------------------------------------------------
// Pass 1 — half-resolution downsample with CoC stored in alpha.
// ---------------------------------------------------------------------------

float4 PSDOFDownsample(PixelShaderInput input) : SV_Target
{
    float2 fullTexel = 1.0f / float2(ViewportSize);
    float2 offsets[4] =
    {
        float2(-0.5f, -0.5f),
        float2( 0.5f, -0.5f),
        float2(-0.5f,  0.5f),
        float2( 0.5f,  0.5f)
    };

    float3 color     = float3(0.0f, 0.0f, 0.0f);
    float  viewDepth = 0.0f;

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float2 sampleUV = saturate(input.UV + offsets[i] * fullTexel);
        color    += ColorTexture.SampleLevel(ColorSampler, sampleUV, 0).rgb;

        float sceneDepth = DepthTexture.Sample(DepthSampler, sampleUV).x;
        viewDepth += abs(ReconstructViewPosition(sampleUV, sceneDepth, InverseProjection).z);
    }

    color     *= 0.25f;
    viewDepth *= 0.25f;

    return float4(color, GetDOFCoC(viewDepth));
}

// ---------------------------------------------------------------------------
// Pass 2 — 3x3 CoC dilation (max filter).
// Expands foreground blur regions outward so near objects bleed correctly.
// Color passes through unchanged; only the alpha (CoC) is dilated.
// ---------------------------------------------------------------------------

float4 PSDOFDilate(PixelShaderInput input) : SV_Target
{
    float4 center = ColorTexture.SampleLevel(ColorSampler, input.UV, 0);
    float  maxCoC = center.a;

    [unroll]
    for (int dy = -1; dy <= 1; dy++)
    {
        [unroll]
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;
            float2 offset = float2(dx, dy) * TexelSize;
            float  tapCoC = ColorTexture.SampleLevel(ColorSampler, saturate(input.UV + offset), 0).a;
            maxCoC = max(maxCoC, tapCoC);
        }
    }

    return float4(center.rgb, maxCoC);
}

// ---------------------------------------------------------------------------
// Pass 3 — single-pass Poisson disc bokeh blur at half resolution.
// Reads from the dilated CoC buffer; radius driven by dilated center CoC.
// Samples are weighted by their own CoC — no depth rejection.
// ---------------------------------------------------------------------------

float4 PSDOFBlur(PixelShaderInput input) : SV_Target
{
    float4 center = ColorTexture.SampleLevel(ColorSampler, input.UV, 0);

    // Dilated CoC drives the disc radius.
    float radius = center.a * DofParams.z;
    if (radius < 0.5f)
        return center;

    float3 accumColor  = center.rgb;
    float  accumWeight = 1.0f;

    [unroll]
    for (int i = 0; i < DOF_DISC_SAMPLES; i++)
    {
        float2 offset = DOF_DISC_OFFSETS[i] * radius * TexelSize;
        float4 tap    = ColorTexture.SampleLevel(ColorSampler, saturate(input.UV + offset), 0);
        // Weight by sample's own CoC (no depth-based rejection).
        // Small baseline prevents zero-CoC samples from being fully ignored.
        float w = tap.a + 0.1f;
        accumColor  += tap.rgb * w;
        accumWeight += w;
    }

    return float4(accumColor / accumWeight, center.a);
}

// ---------------------------------------------------------------------------
// Pass 4 — full-resolution composite.
// ---------------------------------------------------------------------------

float4 PSDOFComposite(PixelShaderInput input) : SV_Target
{
    float4 sharpColor = ColorTexture.Sample(ColorSampler, input.UV);
    float  sceneDepth = DepthTexture.Sample(DepthSampler, input.UV).x;
    float  viewDepth  = abs(ReconstructViewPosition(input.UV, sceneDepth, InverseProjection).z);
    float  coc        = GetDOFCoC(viewDepth);

    if (coc < DOF_COC_EPSILON)
        return sharpColor;

    float3 blurredColor = DistortionTexture.Sample(DistortionSampler, input.UV).rgb;
    return float4(lerp(sharpColor.rgb, blurredColor, coc), sharpColor.a);
}
