// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=1 uniform_buffers=1
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=1

// Backend validation scene: instanced textured quads. Exercises per-vertex and per-instance
// vertex streams, a vertex storage buffer indexed by an instance-stream index, uniforms in
// both stages, a combined texture sampler, blending and a depth target. Drawn by Renderer/RHI/Test/RHITest.cpp.

#include "./ShaderInterop.hlsli"

struct InstanceTint
{
    float4 Color;
};

TEN_VS_UNIFORMS(TestVertexUniforms, 0)
{
    float4x4 Transform;
    float    Time;
    float    Padding0;
    float2   Padding1;
};

TEN_PS_UNIFORMS(TestPixelUniforms, 0)
{
    float4 Fade;
};

TEN_VS_STORAGE_BUFFER(InstanceTint, Tints, 0);
TEN_PS_TEXTURE(Texture2D, DiffuseTexture, 0);

struct VertexInput
{
    TEN_LOCATION(0) float3 Position : TEXCOORD0;
    TEN_LOCATION(1) float2 UV       : TEXCOORD1;
    TEN_LOCATION(2) float4 Offset   : TEXCOORD2; // xy: position, z: depth, w: scale.
    TEN_LOCATION(3) uint   Index    : TEXCOORD3; // Instance index into Tints.
};

struct PixelInput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
    float4 Color    : TEXCOORD1;
};

PixelInput VS(VertexInput input)
{
    float angle = Time + (float)input.Index * 0.4f;
    float2 rotated = float2(
        input.Position.x * cos(angle) - input.Position.y * sin(angle),
        input.Position.x * sin(angle) + input.Position.y * cos(angle));

    float4 position = float4(rotated * input.Offset.w + input.Offset.xy, input.Offset.z, 1.0f);

    PixelInput output;
    output.Position = mul(position, Transform);
    output.UV = input.UV;
    output.Color = Tints[input.Index].Color;
    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    float4 texel = DiffuseTexture.Sample(DiffuseTextureSampler, input.UV);
    return float4(texel.rgb * input.Color.rgb, input.Color.a * Fade.a);
}
