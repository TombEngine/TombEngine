#include "./CBPostProcess.hlsli"
#include "./CBCamera.hlsli"
#include "./Math.hlsli"

#define MAX_RADIUS 24 // 12 => 25 tap (0..12..-12)
#define USE_FAST_BILINEAR 1

struct PostProcessVertexShaderInput
{
    float3 Position : POSITION0;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

struct PixelShaderInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
    float4 PositionCopy : TEXCOORD1;
};

Texture2D ColorTexture : register(t0);
SamplerState ColorSampler : register(s0);

Texture2D GlowTexture : register(t1);
SamplerState GlowSampler : register(s1);

PixelShaderInput VS(PostProcessVertexShaderInput input)
{
    PixelShaderInput output;

    output.Position = float4(input.Position, 1.0f);
    output.UV = input.UV;
    output.Color = input.Color;
    output.PositionCopy = output.Position;

    return output;
}

float4 PSGlowDownscale(PixelShaderInput input) : SV_Target
{
    float2 texel = InvViewSize;
    
    float3 s = 0;
    s += ColorTexture.Sample(ColorSampler, input.UV + float2(-texel.x, -texel.y));
    s += ColorTexture.Sample(ColorSampler, input.UV + float2(+texel.x, -texel.y));
    s += ColorTexture.Sample(ColorSampler, input.UV + float2(-texel.x, +texel.y));
    s += ColorTexture.Sample(ColorSampler, input.UV + float2(+texel.x, +texel.y));
    
    return float4(s * 0.25, 1); 
}

inline float Gaussian(float x, float sigma)
{
    // exp( -x^2 / (2*sigma^2) )
    return exp(-(x * x) / (2.0 * sigma * sigma));
}

float4 PSBlurBilinear(PixelShaderInput input) : SV_Target
{
    int r = clamp(BlurRadius, 0, MAX_RADIUS);
    
    float w0 = Gaussian(0.0, BlurSigma);
    float4 color = ColorTexture.Sample(ColorSampler, input.UV) * w0;
    float weightSum = w0;

    [unroll]
    for (int k = 1; k <= MAX_RADIUS; ++k)
    {
        if (k > r)
            break;

        float wk = Gaussian((float) k, BlurSigma);

        float2 off = BlurDirection * TexelSize * k;
        float4 c1 = ColorTexture.Sample(ColorSampler, input.UV + off);
        float4 c2 = ColorTexture.Sample(ColorSampler, input.UV - off);

        color += (c1 + c2) * wk;
        weightSum += 2.0 * wk;
    }

    return color / weightSum;
}

float4 PSBlurFull(PixelShaderInput input) : SV_Target
{
    int r = clamp(BlurRadius, 0, MAX_RADIUS);

    float4 color = 0;
    float weightSum = 0;

    [unroll]
    for (int k = -MAX_RADIUS; k <= MAX_RADIUS; ++k)
    {
        if (abs(k) > r)
            continue;

        float w = Gaussian((float) k, BlurSigma);
        float2 off = BlurDirection * TexelSize * k;
        color += ColorTexture.Sample(ColorSampler, input.UV + off) * w;
        weightSum += w;
    }

    return color / weightSum;
}

float4 PSGlowBlur(PixelShaderInput input) : SV_Target
{
#if USE_FAST_BILINEAR
    return PSBlurBilinear(input);
#else
    return PSBlurFull(input);
#endif
}

float3 SoftAddBlend(float3 base, float3 add)
{
    return 1.0 - (1.0 - base) * (1.0 - add);
}

float4 PSGlowCombine(PixelShaderInput input) : SV_Target
{
    float3 base = ColorTexture.Sample(ColorSampler, input.UV).rgb;
    float3 glow = GlowTexture.Sample(GlowSampler, input.UV).rgb * GlowIntensity;

    float3 outc = (GlowSoftAdd != 0) ? SoftAddBlend(base, glow)
                                 : saturate(base + glow);

    return float4(outc, 1);
}