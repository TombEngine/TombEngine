// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=2 uniform_buffers=4
// TEN_SHADER: Pixel PS samplers=9 storage_textures=0 storage_buffers=4 uniform_buffers=3 options=NO_ALPHA_TEST,NO_NORMAL_MAP,NO_ORSH_MAP,NO_EMISSIVE_MAP
// TEN_SHADER: Pixel PSDepthNormal samplers=9 storage_textures=0 storage_buffers=3 uniform_buffers=3 options=NO_ALPHA_TEST,NO_NORMAL_MAP,NO_ORSH_MAP,NO_EMISSIVE_MAP

// Room geometry: baked vertex lighting plus the room's dynamic lights with specular, material maps (normal,
// ORSH, emissive, parallax), animated textures, vertex effects, water refraction and distance fog.
// Screen space ambient occlusion comes from the prepass (PSDepthNormal); shadows from the cube shadow map and
// blob spheres; fog bulbs per vertex; caustics in water rooms; decals. Reflections are ported later.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./FrameUniforms.hlsli"
#include "./Lighting.hlsli"
#include "./Materials.hlsli"
#include "./Shadows.hlsli"
#include "./FogBulbs.hlsli"
#include "./Decals.hlsli"

TEN_VS_RAW_BUFFER(GpuAnimatedFrame, AnimatedFrames, 0);
TEN_VS_RAW_BUFFER(GpuAnimatedSet, AnimatedSets, 1);

TEN_PS_RAW_BUFFER(GpuLight, Lights, 9);
TEN_PS_RAW_BUFFER(uint2, LightClusters, 12);

#include "./LightClusters.hlsli"

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
    float4 FogBulbs                 : TEXCOORD9;
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
        GpuAnimatedSet set = TEN_LOAD_RAW(GpuAnimatedSet, AnimatedSets, AnimatedSetIndex);
        if (set.Type == ANIM_TYPE_UVROTATE)
        {
            output.UVScroll = GetUVRotateScroll(TEN_LOAD_RAW(GpuAnimatedFrame, AnimatedFrames, set.FirstFrame), set, InterpolatedFrame);
        }
        else
        {
            uint frameIndex = GetAnimatedFrameIndex(set, Frame, DecodeAnimationFrameOffset(input.AnimationData));
            output.UV = GetFrameCorner(TEN_LOAD_RAW(GpuAnimatedFrame, AnimatedFrames, frameIndex), DecodeIndexInPoly(input.Effects));
        }
    }

    output.Color = float4(Glow(input.Color.rgb, input.Effects, wibble), input.Color.a);
    output.WorldPosition = position;
    output.Normal = input.Normal.xyz;
    output.Tangent = input.Tangent.xyz;
    output.Binormal = cross(input.Normal.xyz, input.Tangent.xyz);
    output.FaceNormal = input.FaceNormal.xyz;
    output.FogBulbs = GetFogBulbs(position, CameraPosition, false);
    output.Fog = GetDistanceFog(position, CameraPosition, FogMinDistance, FogMaxDistance);
    return output;
}

// Triplanar projection of the animated caustics texture, brighter on upward facing surfaces.
float3 GetCaustics(float3 position, float3 normal)
{
    float attenuation = saturate(dot(float3(0.0f, -1.0f, 0.0f), normal));

    float3 blending = normalize(max(abs(normal), 0.00001f));
    blending /= blending.x + blending.y + blending.z;

    float3 p = frac(position / 2048.0f);
    float3 xAxis = CausticsMap.SampleLevel(CausticsMapSampler, CausticsStartUV + p.zy * CausticsSize, 0.0f).rgb;
    float3 yAxis = CausticsMap.SampleLevel(CausticsMapSampler, CausticsStartUV + p.zx * CausticsSize, 0.0f).rgb;
    float3 zAxis = CausticsMap.SampleLevel(CausticsMapSampler, CausticsStartUV + p.yx * CausticsSize, 0.0f).rgb;

    return (xAxis * blending.x + yAxis * blending.y + zAxis * blending.z) * attenuation * 2.0f;
}

// Kept from the original renderer: with mixed light types, point lights get the spot specular term.
// Room lights have no sheen, so without an ORSH map the specular intensity is zero and the terms are left out.
float3 GetRoomLight(GpuLight light, bool onlyPointLights, float3 position, Surface surface)
{
    float3 lighting = float3(0.0f, 0.0f, 0.0f);
    if (onlyPointLights)
    {
        lighting += DoPointLight(position, surface.Normal, light) * COLOR_MODULATION_SCALE * ROOM_LIGHT_COEFF;
#ifndef NO_ORSH_MAP
        lighting += DoSpecularPoint(position, surface.Normal, CameraDirection, light, 0.0f, surface.Specular, surface.Roughness) * COLOR_MODULATION_SCALE;
#endif
    }
    else if (light.Type == LT_POINT || light.Type == LT_SPOT)
    {
        float3 diffuse = (light.Type == LT_POINT) ? DoPointLight(position, surface.Normal, light) : DoSpotLight(position, surface.Normal, light);
        lighting += saturate(diffuse) * COLOR_MODULATION_SCALE * ROOM_LIGHT_COEFF;
#ifndef NO_ORSH_MAP
        lighting += DoSpecularSpot(position, surface.Normal, CameraDirection, light, 0.0f, surface.Specular, surface.Roughness) * COLOR_MODULATION_SCALE * ROOM_LIGHT_COEFF;
#endif
    }

    return lighting;
}

// Lights of one mask word, lowest bit first as in the light list.
float3 GetMaskedRoomLights(uint bits, uint firstLight, bool onlyPointLights, float3 position, Surface surface)
{
    float3 lighting = float3(0.0f, 0.0f, 0.0f);

    [loop]
    while (bits != 0)
    {
        uint light = firstLight + firstbitlow(bits);
        bits &= bits - 1;
        lighting += GetRoomLight(TEN_LOAD_RAW(GpuLight, Lights, LightOffset + light), onlyPointLights, position, surface);
    }

    return lighting;
}

float3 GetRoomLighting(float3 vertexColor, float3 position, Surface surface, float3 tangent, float3 binormal)
{
    float3 lighting = vertexColor * COLOR_MODULATION_SCALE;
    lighting = DoShadow(position, surface.Normal, lighting, -2.5f, BlendMode);
    lighting = DoBlobShadows(position, lighting);

    uint count = min(LightCount & LT_MASK, LIGHTS_PER_DRAW_MAX);
    bool onlyPointLights = (LightCount & ~LT_MASK) == LT_MASK_POINT;

    // Only the lights reaching the light cluster of the position are evaluated.
    uint2 mask;
    if (count > 0 && count <= LIGHT_CLUSTER_LIGHTS_MAX && GetClusterLightMask(position, mask))
    {
        lighting += GetMaskedRoomLights(mask.x, 0, onlyPointLights, position, surface);
        lighting += GetMaskedRoomLights(mask.y, 32, onlyPointLights, position, surface);
    }
    else
    {
        for (uint i = 0; i < count; i++)
            lighting += GetRoomLight(TEN_LOAD_RAW(GpuLight, Lights, LightOffset + i), onlyPointLights, position, surface);
    }

    if (DecalCount > 0)
        lighting *= 1.0f - GetDecalMask(position, tangent, binormal, DecalOffset, DecalCount);

    if (IsCausticsRoom)
        lighting += GetCaustics(position, surface.Normal);

    return lighting + surface.Emissive;
}

float4 PS(PixelInput input) : SV_TARGET
{
    GpuMaterial material = TEN_LOAD_RAW(GpuMaterial, Materials, MaterialIndex);
    Surface surface = SampleSurface(material, ApplyUVScroll(input.UV, input.UVScroll), AlphaThreshold, input.WorldPosition, PixelCameraPosition,
                                    input.Normal, input.Tangent, input.Binormal, input.FaceNormal);
    surface.Color.rgb = ApplyReflections(material, surface.Color.rgb, input.WorldPosition, PixelCameraPosition,
                                         normalize(lerp(input.FaceNormal, surface.Normal, 0.1f)), surface.Specular, View,
                                         InverseViewportSize.y / InverseViewportSize.x);

    float occlusion = surface.Occlusion * GetAmbientOcclusion(material, input.Position.xy * InverseViewportSize, AmbientOcclusionExponent, BlendMode, surface.Color.a);
    float3 lighting = GetRoomLighting(input.Color.rgb, input.WorldPosition, surface, input.Tangent, input.Binormal) - input.FogBulbs.w;
    float4 color = float4(saturate(surface.Color.rgb * lighting * occlusion), surface.Color.a);
    color = ApplyFogBulbs(color, input.FogBulbs.rgb, BlendMode);
    float4 result = ApplyDistanceFog(color, FogColor, input.Fog, BlendMode);
    if (BlendMode == BLENDMODE_DISTORTION)
        return EncodeDistortionPayload(result, dot(input.WorldPosition - PixelCameraPosition, CameraDirection), 1.0f);

    return result;
}

PrepassOutput PSDepthNormal(PixelInput input)
{
    float3 emissive;
    float3 normal = SampleSurfaceNormal(TEN_LOAD_RAW(GpuMaterial, Materials, MaterialIndex), ApplyUVScroll(input.UV, input.UVScroll), AlphaThreshold, input.WorldPosition,
                                        PixelCameraPosition, input.Normal, input.Tangent, input.Binormal, input.FaceNormal, emissive);
    return GetPrepassOutput(EncodeViewNormal(normal, View), emissive, input.Fog, IsOutdoorRoom != 0, input.Position.z);
}
