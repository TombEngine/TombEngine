// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=3 uniform_buffers=2
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=2

// Static meshes and rigid moveable meshes. Every draw is instanced: the instance index stream
// selects the per-instance world matrix, colors and light mode from a storage buffer.
// Dynamic lights are ported in a later milestone; lighting is ambient and vertex color only.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./FrameUniforms.hlsli"

TEN_VS_STORAGE_BUFFER(AnimatedFrameUV, AnimatedFrames, 0);
TEN_VS_STORAGE_BUFFER(AnimatedSet, AnimatedSets, 1);
TEN_VS_STORAGE_BUFFER(ObjectInstance, Instances, 2);

TEN_PS_TEXTURE(Texture2D, ColorMap, 0);

struct VertexInput
{
    TEN_LOCATION(0) float3 Position      : TEXCOORD0;
    TEN_LOCATION(1) float2 UV            : TEXCOORD1;
    TEN_LOCATION(2) float4 Color         : TEXCOORD2;
    TEN_LOCATION(3) uint   Effects       : TEXCOORD3;
    TEN_LOCATION(4) uint   AnimationData : TEXCOORD4;
    TEN_LOCATION(5) uint   InstanceIndex : TEXCOORD5;
};

struct PixelInput
{
    float4 Position                     : SV_POSITION;
    float2 UV                           : TEXCOORD0;
    float4 Color                        : TEXCOORD1;
    float  Fog                          : TEXCOORD2;
    nointerpolation float3 UVScroll     : TEXCOORD3;
    nointerpolation float3 AmbientLight : TEXCOORD4;
    nointerpolation uint   LightMode    : TEXCOORD5;
};

PixelInput VS(VertexInput input)
{
    ObjectInstance instance = Instances[input.InstanceIndex];
    float4 worldPosition = mul(float4(input.Position, 1.0f), instance.World);

    PixelInput output;
    output.Position = mul(worldPosition, ViewProjection);
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

    output.Color = float4(input.Color.rgb * instance.Color.rgb, input.Color.a * instance.Color.a);
    output.AmbientLight = instance.AmbientLight.rgb;
    output.LightMode = instance.LightMode;
    output.Fog = GetDistanceFog(worldPosition.xyz, CameraPosition, FogMinDistance, FogMaxDistance);
    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    float2 uv = ApplyUVScroll(input.UV, input.UVScroll);

    float4 texel = ColorMap.Sample(ColorMapSampler, uv);
    clip(texel.a - AlphaThreshold);

    float3 vertexLight = input.Color.rgb * COLOR_MODULATION_SCALE;
    float3 color = (input.LightMode == LIGHT_MODE_DYNAMIC) ?
        saturate(input.AmbientLight * COLOR_MODULATION_SCALE * texel.rgb * vertexLight) :
        saturate(texel.rgb * vertexLight);

    float4 result = ApplyDistanceFog(float4(color, texel.a), FogColor, input.Fog, BlendMode);
    result.a *= input.Color.a;
    return result;
}
