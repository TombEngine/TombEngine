// Shaders/ConeDepth.fx
#include "./CBCamera.hlsli"
#include "./Math.hlsli"
#include "./VertexInput.hlsli"

cbuffer CConeDepth : register(b8)
{
    int  IsBackPass;    // 0 = Frontfaces (Eintritt), 1 = Backfaces (Austritt)
    float3 _pad0;
};

struct VSOut {
    float4 Pos    : SV_POSITION;
    float3 ViewP  : TEXCOORD0; // view-space Position
};

VSOut VS(VertexShaderInput vin)
{
    VSOut o;
    float4 wpos = float4(vin.Position, 1.0f);

    float4 vpos = mul(wpos, View);
    o.ViewP = vpos.xyz;

    o.Pos = mul(wpos, ViewProjection); // normal zeichnen
    return o;
}

// Ausgabe: lineare View-Z (positiv nach vorne)
float PS(VSOut i) : SV_TARGET
{
    // In LH-Viewspace (DX) zeigt +Z nach vorne. Wir nehmen die Z-Komponente als lineare Tiefe.
    float z = i.ViewP.z;           // >0 vor der Kamera
    return max(z, 0.0f);
}
