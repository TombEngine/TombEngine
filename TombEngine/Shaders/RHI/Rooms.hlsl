// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=2 uniform_buffers=2
// TEN_SHADER: Pixel PS samplers=6 storage_textures=0 storage_buffers=2 uniform_buffers=3
// TEN_SHADER: Pixel PSDepthNormal samplers=6 storage_textures=0 storage_buffers=2 uniform_buffers=3

// Room geometry: baked vertex lighting plus the room's dynamic lights with specular, material maps (normal,
// ORSH, emissive, parallax), animated textures, vertex effects, water refraction and distance fog.
// Screen space ambient occlusion comes from the prepass (PSDepthNormal); shadows from the cube shadow map and
// blob spheres. Reflections, caustics, decals and fog bulbs are ported later.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./FrameUniforms.hlsli"
#include "./Lighting.hlsli"
#include "./Materials.hlsli"
#include "./Shadows.hlsli"

TEN_VS_STORAGE_BUFFER(AnimatedFrameUV, AnimatedFrames, 0);
TEN_VS_STORAGE_BUFFER(AnimatedSet, AnimatedSets, 1);

TEN_PS_STORAGE_BUFFER(GpuLight, Lights, 6);

struct VertexInput
{
    TEN_LOCATION(0) float3 Position      : TEXCOORD0;
    TEN_LOCATION(1) float2 UV            : TEXCOORD1;
    TEN_LOCATION(2) float4 Color         : TEXCOORD2;
    TEN_LOCATION(3) uint   Effects       : TEXCOORD3;
    TEN_LOCATION(4) uint   AnimationData : TEXCOORD4;
    TEN_LOCATION(5) float4 Normal        : TEXCOORD5;
    TEN_LOCATION(6) float4 Tangent       : TEXCOORD6;
    TEN_LOCATION(7) float4 FaceNormal    : TEXCOORD7;
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
    float3 Tangent                  : TEXCOORD6;
    float3 Binormal                 : TEXCOORD7;
    float3 FaceNormal               : TEXCOORD8;
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
    output.Tangent = input.Tangent.xyz;
    output.Binormal = cross(input.Normal.xyz, input.Tangent.xyz);
    output.FaceNormal = input.FaceNormal.xyz;
    output.Fog = GetDistanceFog(position, CameraPosition, FogMinDistance, FogMaxDistance);
    return output;
}

// Legacy quirk kept for parity: with mixed light types, point lights get the spot specular term.
float3 GetRoomLighting(float3 vertexColor, float3 position, Surface surface)
{
    float3 lighting = vertexColor * COLOR_MODULATION_SCALE;
    lighting = DoShadow(position, surface.Normal, lighting, -2.5f, BlendMode);
    lighting = DoBlobShadows(position, lighting);

    uint count = LightCount & LT_MASK;
    bool onlyPointLights = (LightCount & ~LT_MASK) == LT_MASK_POINT;

    for (uint i = 0; i < count; i++)
    {
        GpuLight light = Lights[LightOffset + i];

        if (onlyPointLights)
        {
            lighting += DoPointLight(position, surface.Normal, light) * COLOR_MODULATION_SCALE * ROOM_LIGHT_COEFF;
            lighting += DoSpecularPoint(position, surface.Normal, CameraDirection, light, 0.0f, surface.Specular, surface.Roughness) * COLOR_MODULATION_SCALE;
        }
        else if (light.Type == LT_POINT || light.Type == LT_SPOT)
        {
            float3 diffuse = (light.Type == LT_POINT) ? DoPointLight(position, surface.Normal, light) : DoSpotLight(position, surface.Normal, light);
            float3 specular = DoSpecularSpot(position, surface.Normal, CameraDirection, light, 0.0f, surface.Specular, surface.Roughness);
            lighting += saturate(diffuse) * COLOR_MODULATION_SCALE * ROOM_LIGHT_COEFF + specular * COLOR_MODULATION_SCALE * ROOM_LIGHT_COEFF;
        }
    }

    return lighting + surface.Emissive;
}

float4 PS(PixelInput input) : SV_TARGET
{
    GpuMaterial material = Materials[MaterialIndex];
    Surface surface = SampleSurface(material, ApplyUVScroll(input.UV, input.UVScroll), AlphaThreshold, input.WorldPosition, PixelCameraPosition,
                                    input.Normal, input.Tangent, input.Binormal, input.FaceNormal);

    float occlusion = surface.Occlusion * GetAmbientOcclusion(material, input.Position.xy * InverseViewportSize, AmbientOcclusionExponent, BlendMode, surface.Color.a);
    float3 lighting = GetRoomLighting(input.Color.rgb, input.WorldPosition, surface);
    float4 color = float4(saturate(surface.Color.rgb * lighting * occlusion), surface.Color.a);
    return ApplyDistanceFog(color, FogColor, input.Fog, BlendMode);
}

float4 PSDepthNormal(PixelInput input) : SV_TARGET
{
    float3 normal = SampleSurfaceNormal(Materials[MaterialIndex], ApplyUVScroll(input.UV, input.UVScroll), AlphaThreshold, input.WorldPosition,
                                        PixelCameraPosition, input.Normal, input.Tangent, input.Binormal, input.FaceNormal);
    return float4(EncodeViewNormal(normal, View), 1.0f);
}
