// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=2 uniform_buffers=2
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=2

// Room geometry with baked vertex lighting, animated textures and distance fog.
// Dynamic lights, fog bulbs and materials are ported in later milestones.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"

TEN_VS_UNIFORMS(FrameVertexUniforms, 0)
{
    float4x4 ViewProjection;
    float3   CameraPosition;
    float    FogMinDistance;
    float    FogMaxDistance;
    uint     Frame;
    float    InterpolatedFrame;
    float    FramePadding;
};

TEN_VS_UNIFORMS(DrawVertexUniforms, 1)
{
    int    AnimatedSetIndex;
    float3 DrawPadding;
};

TEN_PS_UNIFORMS(FramePixelUniforms, 0)
{
    float4 FogColor;
};

TEN_PS_UNIFORMS(DrawPixelUniforms, 1)
{
    float AlphaThreshold;
    uint  BlendMode;
    float2 PixelPadding;
};

TEN_VS_STORAGE_BUFFER(AnimatedFrameUV, AnimatedFrames, 0);
TEN_VS_STORAGE_BUFFER(AnimatedSet, AnimatedSets, 1);

TEN_PS_TEXTURE(Texture2D, ColorMap, 0);

struct VertexInput
{
    TEN_LOCATION(0) float3 Position      : TEXCOORD0;
    TEN_LOCATION(1) float2 UV            : TEXCOORD1;
    TEN_LOCATION(2) float4 Color         : TEXCOORD2;
    TEN_LOCATION(3) uint   Effects       : TEXCOORD3;
    TEN_LOCATION(4) uint   AnimationData : TEXCOORD4;
};

struct PixelInput
{
    float4 Position             : SV_POSITION;
    float2 UV                   : TEXCOORD0;
    float4 Color                : TEXCOORD1;
    float  Fog                  : TEXCOORD2;
    nointerpolation float3 UVScroll : TEXCOORD3; // xy: scroll offset, z: 1 when UV rotation applies.
};

PixelInput VS(VertexInput input)
{
    PixelInput output;
    output.Position = mul(float4(input.Position, 1.0f), ViewProjection);
    output.UV = input.UV;
    output.UVScroll = float3(0.0f, 0.0f, 0.0f);

    if (AnimatedSetIndex >= 0)
    {
        AnimatedSet set = AnimatedSets[AnimatedSetIndex];
        if (set.Type == ANIM_TYPE_UVROTATE)
        {
            output.UVScroll = GetUVRotateScroll(AnimatedFrames[set.FirstFrame], set, InterpolatedFrame);
        }
        else
        {
            uint frameIndex = GetAnimatedFrameIndex(set, Frame, DecodeAnimationFrameOffset(input.AnimationData));
            output.UV = GetFrameCorner(AnimatedFrames[frameIndex], DecodeIndexInPoly(input.Effects));
        }
    }

    output.Color = input.Color;
    output.Fog = GetDistanceFog(input.Position, CameraPosition, FogMinDistance, FogMaxDistance);
    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    float2 uv = ApplyUVScroll(input.UV, input.UVScroll);

    float4 color = ColorMap.Sample(ColorMapSampler, uv);
    clip(color.a - AlphaThreshold);

    color.rgb = saturate(color.rgb * input.Color.rgb * COLOR_MODULATION_SCALE);
    return ApplyDistanceFog(color, FogColor, input.Fog, BlendMode);
}
