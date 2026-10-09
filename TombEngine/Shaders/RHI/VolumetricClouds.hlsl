// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PS samplers=3 storage_textures=0 storage_buffers=0 uniform_buffers=4
// TEN_SHADER: Pixel PSCloudComposite samplers=2 storage_textures=0 storage_buffers=0 uniform_buffers=4

// Volumetric clouds of the legacy renderer (Shaders/VolumetricCloudsCore.hlsli): raymarch and composite passes,
// with the legacy cloud uniform block in slot 2 and the camera values it reads in slot 3.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

#define PI 3.1415926535897932384626433832795028841971693993751058209749445923

TEN_PS_UNIFORMS(CloudCameraUniforms, 3)
{
    float4x4 InverseView;
    float4x4 InverseProjection;
    float4   CamPositionWS;
};

#define TEN_VOLUMETRIC_CLOUD_UNIFORMS TEN_PS_UNIFORMS(CBVolumetricCloud, 2)
#include "../CBVolumetricCloud.hlsli"

// Raymarch pass: history and noise textures.
TEN_PS_TEXTURE(Texture2D, CloudTexture, 0);
TEN_PS_TEXTURE(Texture3D<float4>, CloudNoise3D, 1);
TEN_PS_TEXTURE(Texture2D<float2>, CloudWorley2D, 2);

// Composite pass: cloud layer and scene copy.
TEN_PS_TEXTURE(Texture2D, SceneColorTexture, 0);
TEN_PS_TEXTURE(Texture2D, SceneBackgroundTex, 1);

#define SceneColorSamp      SceneColorTextureSampler
#define CloudHistorySamp    CloudTextureSampler
#define SceneBackgroundSamp SceneBackgroundTexSampler
#define NoiseSamp3D         CloudNoise3DSampler
#define NoiseSamp2D         CloudWorley2DSampler

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

#include "../VolumetricCloudsCore.hlsli"
