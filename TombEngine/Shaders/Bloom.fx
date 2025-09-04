// ================================================================
// Bloom LDR (no RT sRGB) – Full FX: VS + PS per tutti gli step
// Steps:
//   1) BrightPass -> BloomSeed (FP16)
//   2) Dual-Kawase Down (2-5 livelli)
//   3) Dual-Kawase Up (additivo)
//   4) Composite su scena LDR (con ritorno a sRGB se RT non-sRGB)
// ================================================================

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

PixelShaderInput VS(PostProcessVertexShaderInput input)
{
    PixelShaderInput output;

    output.Position = float4(input.Position, 1.0f);
    output.UV = input.UV;
    output.Color = input.Color;
    output.PositionCopy = output.Position;

    return output;
}

float4 PS(PixelShaderInput input) : SV_Target
{
    return ColorTexture.Sample(ColorSampler, input.UV);
}

// ===================== Samplers & Buffers =======================
SamplerState sLinearClamp : register(s0);
SamplerState sPointClamp : register(s1);

// b0: parametri bloom
/*cbuffer BloomCB : register(b10)
{
    float threshold; // LDR: ~0.8 .. 0.95 (se bright pass dalla scena)
    float knee; // 0.5 tipico (ammorbidisce la soglia)
    float intensity; // 0.5 .. 1.2 (composite)
    float _pad0;
}

// b1: parametri Kawase (down/up)
cbuffer KawaseCB : register(b11)
{
    float2 texelSize; // 1.0/Width, 1.0/Height del target corrente
    float radius; // 1.0 .. 2.0
    float _pad1;
}*/

// ===================== Utility – sRGB/Linear ====================
// Conversione precisa sRGB <-> Linear (per canali RGB)
float3 SRGBToLinear(float3 c)
{
    float3 lo = c / 12.92;
    float3 hi = pow((c + 0.055) / 1.055, 2.4);
    float3 cond = step(c, 0.04045.xxx);
    return lerp(hi, lo, cond); // if c<=0.04045 -> lo else hi
}

float3 LinearToSRGB(float3 c)
{
    c = saturate(c);
    float3 lo = 12.92 * c;
    float3 hi = 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    float3 cond = step(c, 0.0031308.xxx);
    return lerp(hi, lo, cond); // if c<=0.0031308 -> lo else hi
}

// Soft-threshold (stile ACES/Unreal), adattato a LDR
float3 SoftThreshold(float3 colorLinear, float th, float k)
{
    float y = Luma(colorLinear);
    float kk = th * k;
    float t = saturate((y - th + kk) / max(kk, 1e-6));
    float soft = min(y, th) + (t * t) * (kk * 0.25);
    float w = saturate((y - soft) / max(y, 1e-6));
    return colorLinear * w;
}

// ========================= 1) Bright Pass =======================
// Input: Scene LDR RGBA8 (NON sRGB RT). Se la scena è "baked sRGB", usa PS_BrightPass_sRGB.
// Output: BloomSeed FP16 (RGB in LINEARE).

// t0: Scene LDR (sRGB baked)
Texture2D SceneLDR_sRGB : register(t0);

// Variante: scena già in LINEARE LDR (meno comune)
Texture2D SceneLDR_Linear : register(t0);

#define THRESHOLD 0.85
#define KNEE 0.5
#define RADIUS 1.5
#define INTENSITY 0.8

// Bright pass da scena sRGB baked (linearizza -> soft-threshold -> lineare)
float4 PS_BrightPass_sRGB(PixelShaderInput i) : SV_Target
{
    float3 c_srgb = SceneLDR_sRGB.Sample(sLinearClamp, i.UV).rgb;
    float3 c_lin = SRGBToLinear(c_srgb);
    float3 seed = SoftThreshold(c_lin, THRESHOLD, KNEE);
    return float4(seed, 1.0); // verso RT FP16
}

// Bright pass da scena già lineare
float4 PS_BrightPass_Linear(PixelShaderInput i) : SV_Target
{
    float3 c_lin = SceneLDR_Linear.Sample(sLinearClamp, i.UV).rgb;
    float3 seed = SoftThreshold(c_lin, THRESHOLD, KNEE);
    return float4(seed, 1.0); // verso RT FP16
}

// Pass-through (se hai già un "seed" separato, p.es. specular+emissive): copia 1:1
Texture2D PreSeed_Linear : register(t0);
float4 PSSeedPassthrough(PixelShaderInput i) : SV_Target
{
    return float4(PreSeed_Linear.Sample(sLinearClamp, i.UV).rgb, 1.0);
}

// ====================== 2) Dual-Kawase Down =====================
// Input: t0 = src (LINEARE), Output: FP16 LINEARE
Texture2D SrcTex : register(t0);

float4 PSKawaseDown(PixelShaderInput i) : SV_Target
{
    float2 o = RADIUS * InvViewSize;
    float4 sum = 0;
    sum += SrcTex.Sample(sLinearClamp, i.UV);
    sum += SrcTex.Sample(sLinearClamp, i.UV + float2(o.x, 0));
    sum += SrcTex.Sample(sLinearClamp, i.UV + float2(-o.x, 0));
    sum += SrcTex.Sample(sLinearClamp, i.UV + float2(0, o.y));
    sum += SrcTex.Sample(sLinearClamp, i.UV + float2(0, -o.y));
    return sum * (1.0 / 5.0);
}

// ======================= 3) Dual-Kawase Up ======================
// Input: t0 = lowTex (LINEARE), t1 = highTex (LINEARE), Output: FP16 LINEARE (additivo)
Texture2D LowTex : register(t0);
Texture2D HighTex : register(t1);

float4 PSKawaseUp(PixelShaderInput i) : SV_Target
{
    float2 o = RADIUS * InvViewSize;
    float4 up = 0;
    up += LowTex.Sample(sLinearClamp, i.UV);
    up += LowTex.Sample(sLinearClamp, i.UV + float2(o.x, 0));
    up += LowTex.Sample(sLinearClamp, i.UV + float2(-o.x, 0));
    up += LowTex.Sample(sLinearClamp, i.UV + float2(0, o.y));
    up += LowTex.Sample(sLinearClamp, i.UV + float2(0, -o.y));
    up *= (1.0 / 5.0);

    float4 hi = HighTex.Sample(sLinearClamp, i.UV);
    return up + hi; // somma additiva
}

// ============================ 4) Composite ======================
// Input: t0 = Scene LDR (sRGB baked o lineare), t1 = Bloom (LINEARE)
// Output: RT finale LDR (RGBA8). Poiché il RT **non è sRGB**, offriamo due varianti.

// Caso A (consigliato): scena LDR sRGB baked -> linearizza, somma, ritorno a sRGB manuale
Texture2D SceneLDR_sRGB_Composite : register(t0);
Texture2D BloomTex_Linear : register(t1);

float4 PS_Composite_ToSRGB(PixelShaderInput i) : SV_Target
{
    float3 scene_srgb = SceneLDR_sRGB_Composite.Sample(sLinearClamp, i.UV).rgb;
    float3 scene_lin = SRGBToLinear(scene_srgb);
    float3 bloom_lin = BloomTex_Linear.Sample(sLinearClamp, i.UV).rgb;

    float3 out_lin = scene_lin + bloom_lin * INTENSITY;
    float3 out_srgb = LinearToSRGB(out_lin);
    return float4(out_srgb, 1.0);
}

// Caso B (meno comune): scena già in LINEARE -> somma, ma scrivi direttamente (se farai gamma dopo)
Texture2D SceneLDR_Linear_Composite : register(t0);

float4 PSComposite_LinearOut(PixelShaderInput i) : SV_Target
{
    float3 scene_lin = SceneLDR_Linear_Composite.Sample(sLinearClamp, i.UV).rgb;
    float3 bloom_lin = BloomTex_Linear.Sample(sLinearClamp, i.UV).rgb;
    return float4(scene_lin + bloom_lin * INTENSITY, 1.0);
}
