// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=2 uniform_buffers=2
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=1 uniform_buffers=2

// Room geometry: baked vertex lighting plus the room's dynamic lights, animated textures, vertex effects,
// water refraction and distance fog. Normal maps, specular, shadows, decals and fog bulbs are ported later.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./FrameUniforms.hlsli"
#include "./Lighting.hlsli"

TEN_VS_STORAGE_BUFFER(AnimatedFrameUV, AnimatedFrames, 0);
TEN_VS_STORAGE_BUFFER(AnimatedSet, AnimatedSets, 1);

TEN_PS_TEXTURE(Texture2D, ColorMap, 0);
TEN_PS_STORAGE_BUFFER(GpuLight, Lights, 1);

struct VertexInput
{
    TEN_LOCATION(0) float3 Position      : TEXCOORD0;
    TEN_LOCATION(1) float2 UV            : TEXCOORD1;
    TEN_LOCATION(2) float4 Color         : TEXCOORD2;
    TEN_LOCATION(3) uint   Effects       : TEXCOORD3;
    TEN_LOCATION(4) uint   AnimationData : TEXCOORD4;
    TEN_LOCATION(5) float4 Normal        : TEXCOORD5;
};

struct PixelInput
{
    float4 Position                 : SV_POSITION;
    float2 UV                       : TEXCOORD0;
    float4 Color                    : TEXCOORD1;
    float  Fog                      : TEXCOORD2;
    nointerpolation float3 UVScroll : TEXCOORD3;
    float3 WorldPosition            : TEXCOORD4;
    float3 Normal                   : TEXCOORD5;
};

PixelInput VS(VertexInput input)
{
    // The weight bit keeps portal vertices still and enables refraction.
    float weight = DecodeWeight(input.Effects);
    float wibble = Wibble(input.Effects, DecodeHash(input.AnimationData), InterpolatedFrame);
    float3 position = Move(input.Position, (weight > 0.0f) ? input.Effects : 0, wibble);

    float4 clipPosition = mul(float4(position, 1.0f), ViewProjection);
    if (CameraUnderwater != IsWaterRoom)
    {
        float factor = (InterpolatedFrame + (position.x + position.z) * 0.2f) * 3.14159265359f / 20.0f;
        clipPosition.x += sin(factor) * (clipPosition.z / 1024.0f) * 3.0f * weight;
        clipPosition.y += cos(factor) * (clipPosition.z / 1024.0f) * 3.0f * weight;
    }

    PixelInput output;
    output.Position = clipPosition;
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

    output.Color = float4(Glow(input.Color.rgb, input.Effects, wibble), input.Color.a);
    output.WorldPosition = position;
    output.Normal = input.Normal.xyz;
    output.Fog = GetDistanceFog(position, CameraPosition, FogMinDistance, FogMaxDistance);
    return output;
}

float3 GetRoomLighting(float3 vertexColor, float3 position, float3 normal)
{
    float3 lighting = vertexColor * COLOR_MODULATION_SCALE;

    uint count = LightCount & LT_MASK;
    bool onlyPointLights = (LightCount & ~LT_MASK) == LT_MASK_POINT;

    for (uint i = 0; i < count; i++)
    {
        GpuLight light = Lights[LightOffset + i];

        if (onlyPointLights || light.Type == LT_POINT)
            lighting += DoPointLight(position, normal, light) * COLOR_MODULATION_SCALE * ROOM_LIGHT_COEFF;
        else if (light.Type == LT_SPOT)
            lighting += DoSpotLight(position, normal, light) * COLOR_MODULATION_SCALE * ROOM_LIGHT_COEFF;
    }

    return lighting;
}

float4 PS(PixelInput input) : SV_TARGET
{
    float2 uv = ApplyUVScroll(input.UV, input.UVScroll);

    float4 color = ColorMap.Sample(ColorMapSampler, uv);
    clip(color.a - AlphaThreshold);

    float3 lighting = GetRoomLighting(input.Color.rgb, input.WorldPosition, normalize(input.Normal));
    color.rgb = saturate(color.rgb * lighting);
    return ApplyDistanceFog(color, FogColor, input.Fog, BlendMode);
}
