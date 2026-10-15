// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSDustStorm samplers=2 storage_textures=0 storage_buffers=0 uniform_buffers=4

// Dust storm of the frontend (Shaders/DustStormCore.hlsli): a fullscreen raymarch clamped by the scene depth,
// with the CBDustStorm uniform block in slot 2. The outdoor mask is the alpha of the prepass normals.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

#define TEN_DUST_STORM_BLOCK TEN_PS_UNIFORMS(CBDustStorm, 2)
#include "CBDustStorm.hlsli"

TEN_PS_TEXTURE(Texture2D, DepthTexture, 0);
TEN_PS_TEXTURE(Texture2D, OutdoorMaskTexture, 1);

#define DUST_STORM_SAMPLER(texture) texture##Sampler
#define DUST_STORM_OUTDOOR_CHANNEL  a

struct VSOutput
{
    float4 Position     : SV_POSITION;
    float2 UV           : TEXCOORD0;
    float4 PositionCopy : TEXCOORD1;
};

VSOutput VS(uint vertexId : SV_VertexID)
{
    FullscreenPixelInput fullscreen = FullscreenVS(vertexId);

    VSOutput output;
    output.Position = float4(fullscreen.Position.xy, 1.0f, 1.0f);
    output.UV = fullscreen.UV;
    output.PositionCopy = output.Position;
    return output;
}

#include "DustStormCore.hlsli"
