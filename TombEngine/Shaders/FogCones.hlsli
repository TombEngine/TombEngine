#ifndef FOG_CONES_HLSLI
#define FOG_CONES_HLSLI

#include "./CBCamera.hlsli"
#include "./Math.hlsli"

#define FOG_CONE_MAX 32

struct FogConeGPU
{
    float3 Apex;     float Len;      // Spitze (WS), L‰nge
    float3 Dir;      float R0;       // Achse (unit), Start-Radius
    float3 Color;    float R1;       // Farbe, End-Radius
    float  Density;  float  G;       // Dichte, Anisotropie (0.5..0.85)
    float  NoiseScale; float NoiseSpeed; // NoiseSpeed=0 -> kein ÑFlieﬂenì
};

cbuffer CFogCones : register(b15)
{
    uint       FogConeCount;
    float3     _fogConePad0;
    FogConeGPU FogCones[FOG_CONE_MAX];
}

// kleines station‰res Grain, keine Bewegung
float SmallStaticNoise(float3 p)
{
    uint n = asuint(p.x) * 1973u ^ asuint(p.y) * 9277u ^ asuint(p.z) * 2663u;
    n = (n << 13) ^ n;
    return (1.0 - (float)((n * (n * n * 15731u + 789221u) + 1376312589u) & 0x7fffffff) / 1073741824.0);
}

float PhaseHG(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
    return (1.0 - g2) / max(1e-3, 4.0 * PI * denom);
}

// Einzelnen Kegel auswerten ñ gibt Farbe & Dichte zur¸ck
float4 EvaluateFogConeRGBA(float3 worldPos, float3 V_toCam, FogConeGPU C)
{
    float3 w = worldPos - C.Apex;
    float   d = dot(w, C.Dir);
    if (d < 0.0 || d > C.Len) return 0.0.xxxx;

    float  t = saturate(C.Len > 0.0 ? (d / C.Len) : 0.0);
    float  r = lerp(C.R0, C.R1, t);
    float3 off = w - C.Dir * d;
    float  rho = length(off);
    if (rho > r) return 0.0.xxxx;

    float radial = 1.0 - saturate(rho / max(1e-3, r));
    radial *= radial;

    float axial = pow(1.0 - t, 0.35) * smoothstep(0.0, 1.0, 1.0 - t);

    // Blickrichtung vom Punkt ZUR Kamera => -V ist Kamera->Punkt
    float cosTheta = dot(-V_toCam, C.Dir);
    float phase = PhaseHG(cosTheta, C.G);

    float n = SmallStaticNoise(worldPos * C.NoiseScale);
    float grain = lerp(0.95, 1.05, n);

    float density = C.Density * radial * axial * phase * grain; // skalar
    float3 col = C.Color * density;                           // farbiger Beitrag
    return float4(col, density);
}

// Summe aller Kegel
float4 EvaluateFogConesRGBA(float3 worldPos, float3 V_toCam)
{
    if (FogConeCount == 0) return 0.0.xxxx;

    float3 accCol = 0.0.xxx;
    float  accDen = 0.0;
    [loop]
    for (uint i = 0; i < FogConeCount; ++i)
    {
        float4 c = EvaluateFogConeRGBA(worldPos, V_toCam, FogCones[i]);
        accCol += c.rgb;
        accDen += c.a;
    }
    return float4(accCol, accDen);
}

#endif // FOG_CONES_HLSLI
