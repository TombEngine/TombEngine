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
    float4x4 World;
    uint     IsFogBulbsEnabled;
    uint     IsParaboloid;
    float    Hemisphere; // -1 front, 1 back.
    float    SkyPadding0;
    float4x4 ParaboloidView;
    float    ParaboloidNear;
    float    ParaboloidFar;
    float    SkyPadding1;
    float    SkyPadding2;
};

TEN_PS_UNIFORMS(SkyPixelUniforms, 2)
{
    float4 SkyColor;
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

    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    clip(input.ClipDepth);

    float4 color = ColorMap.Sample(ColorMapSampler, ApplyUVScroll(input.UV, input.UVScroll));
    float3 light = saturate(SkyColor.rgb - input.FogBulbs.w * 1.4f);
    return float4(color.rgb * light + saturate(input.FogBulbs.rgb), color.a * SkyColor.a);
}
