// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=2 uniform_buffers=4
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Classic sky layers and horizon meshes: texture tinted by the layer color, no lighting or depth. The paraboloid
// mode draws them into one hemisphere of the dual paraboloid map of skybox reflective materials (legacy VSSky in
// RoomAmbient.hlsl).

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./FrameUniforms.hlsli"
#include "./FogBulbs.hlsli"

TEN_VS_UNIFORMS(SkyVertexUniforms, 2)
{
    TEN_SKY_VERTEX_UNIFORMS
};

TEN_PS_UNIFORMS(SkyPixelUniforms, 2)
{
    float4 SkyColor;
};

TEN_VS_STORAGE_BUFFER(GpuAnimatedFrame, AnimatedFrames, 0);
TEN_VS_STORAGE_BUFFER(GpuAnimatedSet, AnimatedSets, 1);

TEN_PS_TEXTURE(Texture2DArray, ColorMap, 0); // Sky texture or horizon page; TextureLayer selects the layer.

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
    float4 Position                 : SV_POSITION;
    float2 UV                       : TEXCOORD0;
    nointerpolation float3 UVScroll : TEXCOORD1;
    float4 FogBulbs                 : TEXCOORD2;
    float  ClipDepth                : TEXCOORD3; // Negative behind the paraboloid hemisphere.
};

PixelInput VS(VertexInput input)
{
    float3 worldPosition = mul(float4(input.Position, 1.0f), World).xyz;

    PixelInput output;
    output.Position = mul(float4(worldPosition, 1.0f), ViewProjection);
    output.ClipDepth = 1.0f;

    if (IsParaboloid)
    {
        float4 position = mul(float4(worldPosition, 1.0f), ParaboloidView);
        position.z *= Hemisphere;

        // Denominators are clamped: infinite or NaN positions, harmless on DX11, can lose the Vulkan device.
        float distance = max(length(position.xyz), 1e-4f);
        position.xyz /= distance;
        output.ClipDepth = position.z;
        output.Position = float4(position.xy / max(position.z + 1.0f, 1e-4f), (distance - ParaboloidNear) / (ParaboloidFar - ParaboloidNear), 1.0f);
    }

    output.FogBulbs = IsFogBulbsEnabled ? GetFogBulbs(worldPosition, CameraPosition, true) : float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.UV = input.UV;
    output.UVScroll = float3(0.0f, 0.0f, 0.0f);

    if (AnimatedSetIndex >= 0)
    {
        GpuAnimatedSet set = AnimatedSets[AnimatedSetIndex];
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

    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    clip(input.ClipDepth);

    float4 color = ColorMap.Sample(ColorMapSampler, float3(ApplyUVScroll(input.UV, input.UVScroll), TextureLayer));
    float3 light = saturate(SkyColor.rgb - input.FogBulbs.w * 1.4f);
    return float4(color.rgb * light + saturate(input.FogBulbs.rgb), color.a * SkyColor.a);
}
