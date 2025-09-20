// Shaders/ConeComposite.fx
#include "./CBCamera.hlsli"
#include "./Math.hlsli"

Texture2D FrontZ : register(t0);   // R32_FLOAT, View-Z Eintritt
Texture2D BackZ  : register(t1);   // R32_FLOAT, View-Z Austritt
SamplerState sLinClamp : register(s3);

cbuffer CVolCone : register(b8)
{
    float4x4 ConeViewToLocal;  // ViewSpace -> ConeLocal (Apex = 0, +Z = Achse)
    float3   ConeColor;  float Intensity;
    float    ConeLen;    float ConeR0;  float ConeR1;  float G; // G=Anisotropie 0.5..0.85
};

// Vollbild-Quad
struct VSOut { float4 Pos:SV_POSITION; float2 UV:TEXCOORD0; };

VSOut VS(uint id : SV_VertexID)
{
    VSOut o;
    // Fullscreen triangle
    float2 verts[3] = { float2(-1,-1), float2(3,-1), float2(-1,3) };
    o.Pos = float4(verts[id], 0, 1);
    o.UV = o.Pos.xy * 0.5f + 0.5f;
    return o;
}

static float2 rot2(float2 p, float a) {
    float s = sin(a), c = cos(a);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

// Rechne ViewRay aus UV (kein World nötig)
float3 GetViewRay(float2 uv)
{
    float2 ndc = uv * 2.0f - 1.0f;
    float4 pVS = mul(float4(ndc, 1.0f, 1.0f), InverseProjection);
    // In Viewspace auf z=+1 normieren (DX-LH)
    return normalize(float3(pVS.xy, 1.0f));
}

float PhaseHG(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = pow(1.0f + g2 - 2.0f * g * cosTheta, 1.5f);
    return (1.0f - g2) / max(1e-3, 4.0f * PI * denom);
}

float4 PS(VSOut i) : SV_TARGET
{
    // Front/Back Depth (View-Z)
    float z0 = FrontZ.SampleLevel(sLinClamp, i.UV, 0).r;
    float z1 = BackZ.SampleLevel(sLinClamp, i.UV, 0).r;

    // Kein Treffer / falsch herum
    if (z0 <= 0.0f || z1 <= 0.0f || z1 <= z0) discard;

    // View-Ray
    float3 dVS = GetViewRay(i.UV);
    if (dVS.z <= 1e-5) discard; // nur vorwärts

    // Parametrisierung entlang des View-Rays (t ist Viewspace-Distanz)
    float t0 = z0 / dVS.z;
    float t1 = z1 / dVS.z;

    // Raymarch
    const int   STEPS = 32;
    float       dt = (t1 - t0) / STEPS;
    float       smoke = 0.0f;

    // Blickwinkel-Faktor (annähernd konstant pro Pixel)
    // Cone-Achse in Viewspace ist die 3. Zeile der Inversen von ConeViewToLocal (oder wir rechnen über einen Richtungsvektor)
    // Wir nehmen hier die Achse indirekt: Transformiere einen Einheitsvektor +Z lokal nach View, indem wir inverse-rotation aus ConeViewToLocal abschätzen:
    float3 axisVS = normalize((float3)mul(float3(0,0,1), (float3x3)transpose((float3x3)ConeViewToLocal))); // nur Rotation
    float  cosTh = dot(-normalize(dVS), normalize(axisVS));
    float  phase = PhaseHG(cosTh, G);

    [loop]
    for (int s = 0; s < STEPS; ++s)
    {
        float t = t0 + (s + 0.5) * dt;
        float3 Pvs = dVS * t; // Punkt in Viewspace

        // nach Cone-Local
        float4 Pl = mul(float4(Pvs, 1.0f), ConeViewToLocal);
        float  z = Pl.z;
        if (z < 0.0f || z > ConeLen) continue;

        float rAtZ = lerp(ConeR0, ConeR1, z / ConeLen);
        float rho = length(Pl.xy);
        if (rho > rAtZ) continue; // Sicherheitsgurt, sollte innerhalb z0..z1 nicht passieren

        // Dichteprofil: zur Achse dichter, zum Ende ausblendend
        float axial = pow(1.0 - saturate(z / ConeLen), 0.5);
        float radial = 1.0 - saturate(rho / max(1e-3, rAtZ));
        float density = axial * radial;

        // Leichtes statisches Detail via prozeduralem Noise im lokalen Raum (ohne Scroll!)
        float2 uvN = rot2(Pl.xy / max(4.0, rAtZ * 2.0), 0.35); // sehr langsam, praktisch statisch
        float n = 0.6 * FractalNoise(uvN * 2.0) + 0.4 * FractalNoise(uvN * 4.0);
        density *= lerp(0.85, 1.15, n);

        smoke += density * dt;
    }

    float3 col = ConeColor * (Intensity * phase) * smoke;
    return float4(col, 1.0f); // Additives Blending nutzen
}
