// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=1 uniform_buffers=2
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=2

// Billboards and 3D quads share one path: corners are expanded to world space on the CPU, each sprite
// is one instance of six vertices, and the corner index comes from a static vertex stream.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./FrameUniforms.hlsli"

// Colors are real uint fields: integer bits stored in float components can be altered by drivers
// that canonicalize NaN patterns (opaque RGBA8 colors are NaN when read as float).
struct SpriteCorner
{
    float3 Position;
    uint   Color; // Packed RGBA8.
};

struct SpriteInstance
{
    SpriteCorner Corners[4];
    float4       UVX;
    float4       UVY;
};

TEN_VS_STORAGE_BUFFER(SpriteInstance, Sprites, 0);

TEN_PS_TEXTURE(Texture2D, ColorMap, 0);

struct VertexInput
{
    TEN_LOCATION(0) uint Corner        : TEXCOORD0;
    TEN_LOCATION(1) uint InstanceIndex : TEXCOORD1;
};

struct PixelInput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
    float4 Color    : TEXCOORD1;
    float  Fog      : TEXCOORD2;
};

float4 UnpackColor(uint packedColor)
{
    return float4(packedColor & 0xFF, (packedColor >> 8) & 0xFF, (packedColor >> 16) & 0xFF, packedColor >> 24) / 255.0f;
}

PixelInput VS(VertexInput input)
{
    SpriteInstance sprite = Sprites[input.InstanceIndex];
    SpriteCorner corner = sprite.Corners[input.Corner];

    PixelInput output;
    output.Position = mul(float4(corner.Position, 1.0f), ViewProjection);
    output.UV = float2(sprite.UVX[input.Corner], sprite.UVY[input.Corner]);
    output.Color = UnpackColor(corner.Color);
    output.Fog = GetDistanceFog(corner.Position, CameraPosition, FogMinDistance, FogMaxDistance);
    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    float4 color = ColorMap.Sample(ColorMapSampler, input.UV) * input.Color;
    clip(color.a - AlphaThreshold);

    color.rgb = saturate(color.rgb);
    return ApplyDistanceFog(color, float4(0.0f, 0.0f, 0.0f, 0.0f), input.Fog, BlendMode);
}
