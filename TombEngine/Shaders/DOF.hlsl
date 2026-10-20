// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSDownsample samplers=2 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSFarBlur samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSNearDilate samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSNearBlur samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Depth of field (SceneRendererDOF.cpp): half resolution downsample with the signed circle
// of confusion packed in alpha, far blur, near circle of confusion dilation and near blur. The post-process
// composites the two blurred images over the sharp scene.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

#define DOF_MODE_NONE          0
#define DOF_MODE_FRONT         2
#define DOF_MODE_BACK          3
#define DOF_COC_EPSILON        0.025f
#define DOF_DISC_SAMPLES       12
#define DOF_DILATE_SAMPLES     8
#define DOF_DEPTH_REJECT_SCALE 3.0f
#define DOF_MAX_BOKEH_RADIUS   8.0f

TEN_PS_UNIFORMS(DofUniforms, 2)
{
    float  FocusDistance;
    float  FocusRange;
    float  Strength;
    uint   Mode;
    float2 TexelSize;     // Of the half resolution targets.
    float2 FullTexelSize; // Of the scene.
    float  NearPlane;
    float  FarPlane;
    float  DofPadding0;
    float  DofPadding1;
};

TEN_PS_TEXTURE(Texture2D, SourceMap, 0);
TEN_PS_TEXTURE(Texture2D, DepthMap, 1);

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
    float2(-0.7916f, -0.5977f)
};

FullscreenPixelInput VS(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

float4 SampleSource(float2 uv)
{
    return SourceMap.SampleLevel(SourceMapSampler, saturate(uv), 0.0f);
}

// Negative is near (foreground), positive is far (background).
float GetSignedCoC(float viewDepth)
{
    if (Mode == DOF_MODE_NONE)
        return 0.0f;

    float signedCoC = clamp((viewDepth - FocusDistance) / max(FocusRange, 1.0f), -1.0f, 1.0f);
    signedCoC = sign(signedCoC) * pow(abs(signedCoC), 1.3f);

    if (Mode == DOF_MODE_FRONT)
        signedCoC = min(signedCoC, 0.0f);
    else if (Mode == DOF_MODE_BACK)
        signedCoC = max(signedCoC, 0.0f);

    return signedCoC;
}

float UnpackSignedCoC(float packedCoC)
{
    return (packedCoC - 0.5f) * 2.0f;
}

float GetBokehRadius(float coc)
{
    return coc * saturate(Strength) * DOF_MAX_BOKEH_RADIUS;
}

// Averages four scene texels and their view depths (the depth buffer value mapped back to view space distance),
// then packs the signed circle of confusion from [-1, 1] into the alpha in [0, 1].
float4 PSDownsample(FullscreenPixelInput input) : SV_Target
{
    const float2 OFFSETS[4] = { float2(-0.5f, -0.5f), float2(0.5f, -0.5f), float2(-0.5f, 0.5f), float2(0.5f, 0.5f) };

    float3 color = 0.0f;
    float viewDepth = 0.0f;

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float2 uv = saturate(input.UV + OFFSETS[i] * FullTexelSize);
        color += SampleSource(uv).rgb;

        float depth = DepthMap.SampleLevel(DepthMapSampler, uv, 0.0f).x;
        viewDepth += (NearPlane * FarPlane) / (FarPlane - depth * (FarPlane - NearPlane));
    }

    return float4(color * 0.25f, 0.5f + 0.5f * GetSignedCoC(viewDepth * 0.25f));
}

// Disc blur of the texels on the same side of the focus plane; texels with a clearly different circle of
// confusion are weighted down. The sign selects the far (1) or near (-1) side.
float4 BlurDisc(float2 uv, float side)
{
    float4 center = SampleSource(uv);
    float centerCoC = max(0.0f, UnpackSignedCoC(center.a) * side);
    float radius = GetBokehRadius(centerCoC);
    if (radius < 0.5f)
        return float4(center.rgb, 0.0f);

    float3 color = center.rgb * centerCoC;
    float weightSum = centerCoC;
    float rejectThreshold = max(centerCoC * DOF_DEPTH_REJECT_SCALE, DOF_COC_EPSILON);

    [unroll]
    for (int i = 0; i < DOF_DISC_SAMPLES; i++)
    {
        float4 tap = SampleSource(uv + DOF_DISC_OFFSETS[i] * radius * TexelSize);
        float tapCoC = max(0.0f, UnpackSignedCoC(tap.a) * side);

        float weight = (abs(tapCoC - centerCoC) < rejectThreshold) ? (tapCoC + DOF_COC_EPSILON) : DOF_COC_EPSILON;
        color += tap.rgb * weight;
        weightSum += weight;
    }

    return float4(color / max(weightSum, DOF_COC_EPSILON), centerCoC);
}

// Outputs the blurred color with the far circle of confusion in alpha, used as the composite weight.
float4 PSFarBlur(FullscreenPixelInput input) : SV_Target
{
    return BlurDisc(input.UV, 1.0f);
}

// Minimum filter of the packed circle of confusion, so foreground edges blur as much as their interior.
float4 PSNearDilate(FullscreenPixelInput input) : SV_Target
{
    float4 center = SampleSource(input.UV);
    float minCoC = center.a;
    float dilateRadius = clamp(saturate(Strength) * DOF_MAX_BOKEH_RADIUS * 2.0f, 1.0f, 16.0f);

    [unroll]
    for (int i = 0; i < DOF_DILATE_SAMPLES; i++)
        minCoC = min(minCoC, SampleSource(input.UV + DOF_DISC_OFFSETS[i] * dilateRadius * TexelSize).a);

    return float4(center.rgb, minCoC);
}

float4 PSNearBlur(FullscreenPixelInput input) : SV_Target
{
    return BlurDisc(input.UV, -1.0f);
}
