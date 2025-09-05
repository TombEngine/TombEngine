#include "./CBPostProcess.hlsli"
#include "./CBCamera.hlsli"
#include "./Math.hlsli"

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

float4 PSGlowBlur(PixelShaderInput input) : SV_Target
{
    // Parametri
    const float Radius = 3.5f; // in "pixel full-res" (coerente con /4 sotto). Prova 3.5–6.0 per glow molto ampio
    const float Intensity = 1.0f;

    // Pass lavori su RT downscaled di 4x  compensa gli offset con /4
    const float2 texel = InvViewSize / 4.0f;

    const float2 r1 = Radius * texel; // anello 1
    const float2 r2 = r1 * 2.0; // anello 2
    const float2 r3 = r1 * 3.0; // anello 3 (facoltativo, pesa poco)

    // Pesi (grossomodo gauss): somma 1, normalizzata sotto
    const float w0 = 0.06;
    const float w1c = 0.10; // cross r1
    const float w1d = 0.06; // diag  r1
    const float w2c = 0.05; // cross r2
    const float w2d = 0.025; // diag  r2
    const float w3c = 0.0125; // cross r3 (facoltativo)

    float3 c = 0;

    // Centro
    c += ColorTexture.Sample(ColorSampler, input.UV).rgb * w0;

    // r1 - cross
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(+r1.x, 0)).rgb * w1c;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(-r1.x, 0)).rgb * w1c;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(0, +r1.y)).rgb * w1c;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(0, -r1.y)).rgb * w1c;

    // r1 - diagonali
    c += ColorTexture.Sample(ColorSampler, input.UV + r1).rgb * w1d;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(+r1.x, -r1.y)).rgb * w1d;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(-r1.x, +r1.y)).rgb * w1d;
    c += ColorTexture.Sample(ColorSampler, input.UV - r1).rgb * w1d;

    // r2 - cross
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(+r2.x, 0)).rgb * w2c;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(-r2.x, 0)).rgb * w2c;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(0, +r2.y)).rgb * w2c;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(0, -r2.y)).rgb * w2c;

    // r2 - diagonali
    c += ColorTexture.Sample(ColorSampler, input.UV + r2).rgb * w2d;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(+r2.x, -r2.y)).rgb * w2d;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(-r2.x, +r2.y)).rgb * w2d;
    c += ColorTexture.Sample(ColorSampler, input.UV - r2).rgb * w2d;

    // r3 - cross (opzionale: commenta se vuoi risparmiare)
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(+r3.x, 0)).rgb * w3c;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(-r3.x, 0)).rgb * w3c;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(0, +r3.y)).rgb * w3c;
    c += ColorTexture.Sample(ColorSampler, input.UV + float2(0, -r3.y)).rgb * w3c;

    // Normalizzazione (somma pesi 1.05)
    const float totalW = w0 + 4.0 * (w1c + w1d + w2c + w2d) + 4.0 * w3c;
    c *= (Intensity / totalW);

    return float4(c, 1.0);
}


float3 SoftAddBlend(float3 base, float3 add)   // Screen-like
{
    return 1.0 - (1.0 - base) * (1.0 - add);
}

float4 PSGlowCombine(PixelShaderInput input) : SV_Target
{
    float GlowIntensity = 1.2;
    int SoftAdd = 1;
    
    float3 base = ColorTexture.Sample(ColorSampler, input.UV).rgb;
    float3 glow = GlowTexture.Sample(GlowSampler, input.UV).rgb * GlowIntensity;

    float3 outc = (SoftAdd != 0) ? SoftAddBlend(base, glow)
                                 : saturate(base + glow);

    return float4(outc, 1);
}