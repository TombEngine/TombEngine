// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSGodRay samplers=4 storage_textures=0 storage_buffers=0 uniform_buffers=4
// TEN_SHADER: Pixel PSGodRayComposite samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=4

// God rays (SceneRendererSky.cpp), implemented in GodRayCore.hlsli: radial blur at half resolution and additive
// composite, with the CBGodRay uniform block in slot 2.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

#define TEN_GOD_RAY_BLOCK TEN_PS_UNIFORMS(CBGodRay, 2)
#include "CBGodRay.hlsli"

// Radial blur: cloud layers, horizon silhouette and scene. The composite reads the god rays from slot 0.
TEN_PS_TEXTURE(Texture2D, CloudTexture, 0);
TEN_PS_TEXTURE(Texture2D, CloudTextureB, 1);
TEN_PS_TEXTURE(Texture2D, HorizonMaskTexture, 2);
TEN_PS_TEXTURE(Texture2D, SceneTexture, 3);

#define GOD_RAY_SAMPLER(texture) texture##Sampler

struct VSOutput
{
    float4 Position     : SV_POSITION;
    float2 UV           : TEXCOORD0;
    float4 PositionCopy : TEXCOORD1;
};

// Fullscreen triangle on the far plane; PositionCopy passes the clip position to the pixel shaders.
VSOutput VS(uint vertexId : SV_VertexID)
{
    FullscreenPixelInput fullscreen = FullscreenVS(vertexId);

    VSOutput output;
    output.Position = float4(fullscreen.Position.xy, 1.0f, 1.0f);
    output.UV = fullscreen.UV;
    output.PositionCopy = output.Position;
    return output;
}

#include "GodRayCore.hlsli"
