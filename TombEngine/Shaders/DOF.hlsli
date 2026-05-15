// Depth-of-field: downsample with packed signed CoC, separate near/far blur, composite.
// Should be included near the top of PostProcess.hlsl, after CB and texture declarations.
//
// DofParams layout (float4):
//   x = focus distance (view-space units)
//   y = focus range    (view-space units)
//   z = normalized bokeh strength in [0, 1]
//   w = DOFMode:  0 = None, 1 = Full, 2 = Front, 3 = Back
//
// Downsample alpha encoding: 0.5 + 0.5 * clamp(signedCoC, -1, 1)
//   signedCoC < 0 → foreground (near)  → nearCoC = saturate(2*(0.5 - A))
//   signedCoC > 0 → background (far)   → farCoC  = saturate(2*(A - 0.5))

// ---------------------------------------------------------------------------
// Constants and kernel.
// ---------------------------------------------------------------------------

#define DOF_COC_EPSILON        0.025f
#define DOF_DISC_SAMPLES       12
#define DOF_DEPTH_REJECT_SCALE 3.0f  // Rejection threshold relative to center CoC.
#define DOF_MAX_BOKEH_RADIUS   8.0f
#define DOF_DILATE_SAMPLES     8

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

// Returns the raw signed CoC (negative = near/foreground, positive = far/background).
float GetSignedCoC(float viewDepth)
{
    int mode = (int)DofParams.w;
    if (mode == 0)
        return 0.0f;

    float signedCoC = (viewDepth - DofParams.x) / max(DofParams.y, 1.0f);
	float expandedCoC = pow(abs(clamp(signedCoC, -1.0f, 1.0f)), 1.3f);
	signedCoC = sign(signedCoC) * expandedCoC;

    if (mode == 2) // Front only: clamp so background has no CoC.
        signedCoC = min(signedCoC, 0.0f);
    else if (mode == 3) // Back only: clamp so foreground has no CoC.
        signedCoC = max(signedCoC, 0.0f);

    return signedCoC;
}

// Unpack packed CoC from downsample alpha to signed CoC.
float UnpackSignedCoC(float packedAlpha)
{
    return (packedAlpha - 0.5f) * 2.0f;
}

float GetBokehRadius(float coc)
{
    return coc * saturate(DofParams.z) * DOF_MAX_BOKEH_RADIUS;
}

// ---------------------------------------------------------------------------
// Pass 1 — half-resolution downsample with packed signed CoC in alpha.
// Alpha = 0.5 + 0.5 * signedCoC
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
        color += ColorTexture.SampleLevel(LinearClampSampler, sampleUV, 0).rgb;

        float sceneDepth = DepthTexture.Sample(PointWrapSampler, sampleUV).x;
        viewDepth += abs(ReconstructViewPosition(sampleUV, sceneDepth, InverseProjection).z);
    }

    color *= 0.25f;
    viewDepth *= 0.25f;

    float signedCoC = GetSignedCoC(viewDepth);
    float packedCoC = 0.5f + 0.5f * signedCoC;

    return float4(color, packedCoC);
}

// ---------------------------------------------------------------------------
// Pass 2 — far (background) blur at half resolution.
// Reads from the undilated downsample RT; blurs pixels with positive CoC.
// Depth rejection: samples with very different CoC are weighted down.
// Output alpha = farCoC of center sample.
// ---------------------------------------------------------------------------

float4 PSDOFFarBlur(PixelShaderInput input) : SV_Target
{
    float4 center       = ColorTexture.SampleLevel(LinearClampSampler, input.UV, 0);
    float  centerSigned = UnpackSignedCoC(center.a);
    float  centerFarCoC = max(0.0f, centerSigned);
    float  radius       = GetBokehRadius(centerFarCoC);

    if (radius < 0.5f)
        return float4(center.rgb, 0.0f);

    float3 accumColor  = center.rgb * centerFarCoC;
    float  accumWeight = centerFarCoC;

    float rejectThreshold = max(centerFarCoC * DOF_DEPTH_REJECT_SCALE, DOF_COC_EPSILON);

    [unroll]
    for (int i = 0; i < DOF_DISC_SAMPLES; i++)
    {
        float2 offset    = DOF_DISC_OFFSETS[i] * radius * TexelSize;
        float4 tap       = ColorTexture.SampleLevel(LinearClampSampler, saturate(input.UV + offset), 0);
        float  tapSigned = UnpackSignedCoC(tap.a);
        float  tapFarCoC = max(0.0f, tapSigned);

        // Reject samples that are clearly not in the far-blur region.
        float w = (abs(tapFarCoC - centerFarCoC) < rejectThreshold) ? (tapFarCoC + DOF_COC_EPSILON) : DOF_COC_EPSILON;
        accumColor  += tap.rgb * w;
        accumWeight += w;
    }

    return float4(accumColor / max(accumWeight, DOF_COC_EPSILON), centerFarCoC);
}

// ---------------------------------------------------------------------------
// Pass 3 — near CoC dilation (min-filter on packed alpha).
// Expanding the near CoC outward ensures foreground object edges get the
// same blur radius as the interior, preventing sharp silhouette lines.
// Dilation radius scales with DofParams.z (bokeh strength) so stronger
// blur settings also widen the edge coverage proportionally.
// Color passes through from the center sample unchanged.
// ---------------------------------------------------------------------------

// Reuse the bokeh disc offsets for the dilation neighbourhood.
float4 PSDOFNearDilate(PixelShaderInput input) : SV_Target
{
    float4 center   = ColorTexture.SampleLevel(LinearClampSampler, input.UV, 0);
    float  minAlpha = center.a;

    // Scale dilation radius with bokeh strength; clamp so it stays reasonable.
    float dilateRadius = clamp(saturate(DofParams.z) * DOF_MAX_BOKEH_RADIUS * 2.0f, 1.0f, 16.0f);

    [unroll]
    for (int i = 0; i < DOF_DILATE_SAMPLES; i++)
    {
        float2 offset   = DOF_DISC_OFFSETS[i] * dilateRadius * TexelSize;
        float  tapAlpha = ColorTexture.SampleLevel(LinearClampSampler, saturate(input.UV + offset), 0).a;
		
        // Lower packed alpha = higher near CoC; min expands foreground blur outward.
        minAlpha = min(minAlpha, tapAlpha);
    }

    return float4(center.rgb, minAlpha);
}

// ---------------------------------------------------------------------------
// Pass 4 — near (foreground) blur at half resolution.
// Reads from the dilated near-CoC RT; blurs pixels with negative CoC.
// Depth rejection: samples with very different CoC are weighted down.
// Output alpha = nearCoC of center sample.
// ---------------------------------------------------------------------------

float4 PSDOFNearBlur(PixelShaderInput input) : SV_Target
{
    float4 center        = ColorTexture.SampleLevel(LinearClampSampler, input.UV, 0);
    float  centerSigned  = UnpackSignedCoC(center.a);
    float  centerNearCoC = max(0.0f, -centerSigned);
    float  radius        = GetBokehRadius(centerNearCoC);

    if (radius < 0.5f)
        return float4(center.rgb, 0.0f);

    float3 accumColor  = center.rgb * centerNearCoC;
    float  accumWeight = centerNearCoC;

    float rejectThreshold = max(centerNearCoC * DOF_DEPTH_REJECT_SCALE, DOF_COC_EPSILON);

    [unroll]
    for (int i = 0; i < DOF_DISC_SAMPLES; i++)
    {
        float2 offset     = DOF_DISC_OFFSETS[i] * radius * TexelSize;
        float4 tap        = ColorTexture.SampleLevel(LinearClampSampler, saturate(input.UV + offset), 0);
        float  tapSigned  = UnpackSignedCoC(tap.a);
        float  tapNearCoC = max(0.0f, -tapSigned);

        float w = (abs(tapNearCoC - centerNearCoC) < rejectThreshold) ? (tapNearCoC + DOF_COC_EPSILON) : DOF_COC_EPSILON;
        accumColor  += tap.rgb * w;
        accumWeight += w;
    }

    return float4(accumColor / max(accumWeight, DOF_COC_EPSILON), centerNearCoC);
}

// ---------------------------------------------------------------------------
// Pass 5 — full-resolution composite.
// t0  (ColorTexture)    = sharp full-res image
// t16 (NearBlurTexture) = far blur (half-res, alpha = farCoC)
// t17 (FarBlurTexture)  = near blur (half-res, alpha = nearCoC)
//
// Composite order: sharp → apply far blur → apply near blur on top.
// ---------------------------------------------------------------------------

float4 PSDOFComposite(PixelShaderInput input) : SV_Target
{
    float4 sharpColor = ColorTexture.Sample(LinearClampSampler, input.UV);
    float4 nearBlur   = NearBlurTexture.Sample(LinearClampSampler, input.UV);
    float4 farBlur    = FarBlurTexture.Sample(LinearClampSampler, input.UV);

    float farCoC  = saturate(farBlur.a);
    float nearCoC = saturate(nearBlur.a);

    float3 result = sharpColor.rgb;
    result = lerp(result, farBlur.rgb,  farCoC);
    result = lerp(result, nearBlur.rgb, nearCoC);

    return float4(result, sharpColor.a);
}
