// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=4 uniform_buffers=4
// TEN_SHADER: Pixel PS samplers=9 storage_textures=0 storage_buffers=3 uniform_buffers=3 permutations=NO_ALPHA_TEST
// TEN_SHADER: Pixel PSDepthNormal samplers=9 storage_textures=0 storage_buffers=3 uniform_buffers=3 permutations=NO_ALPHA_TEST
// TEN_SHADER: Pixel PSShadow samplers=9 storage_textures=0 storage_buffers=3 uniform_buffers=3
// TEN_SHADER: Vertex VSInventory samplers=0 storage_textures=0 storage_buffers=4 uniform_buffers=4
// TEN_SHADER: Pixel PSInventory samplers=9 storage_textures=0 storage_buffers=3 uniform_buffers=3

// Static meshes and rigid moveable meshes. Every draw is instanced: the instance index stream
// selects the per-instance world matrix, colors and light mode from a storage buffer.
// Lighting follows the legacy CombineLights / StaticLight model with material maps and specular.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./FrameUniforms.hlsli"
#include "./Lighting.hlsli"
#include "./Materials.hlsli"
#include "./Shadows.hlsli"
#include "./FogBulbs.hlsli"
#include "./Decals.hlsli"
#include "./Skinning.hlsli"

TEN_VS_STORAGE_BUFFER(GpuAnimatedFrame, AnimatedFrames, 0);
TEN_VS_STORAGE_BUFFER(GpuAnimatedSet, AnimatedSets, 1);
TEN_VS_STORAGE_BUFFER(GpuObjectInstance, Instances, 2);
TEN_VS_STORAGE_BUFFER(BoneTransform, Bones, 3);

TEN_PS_STORAGE_BUFFER(GpuLight, Lights, 9);

struct VertexInput
{
    TEN_LOCATION(0) float3 Position      : TEXCOORD0;
    TEN_LOCATION(1) float2 UV            : TEXCOORD1;
    TEN_LOCATION(2) float4 Color         : TEXCOORD2;
    TEN_LOCATION(3) uint   Effects       : TEXCOORD3;
    TEN_LOCATION(4) uint   AnimationData : TEXCOORD4;
    TEN_LOCATION(5) uint   InstanceIndex : TEXCOORD5;
    TEN_LOCATION(6) float4 Normal        : TEXCOORD6;
    TEN_LOCATION(7) uint4  BoneIndices   : TEXCOORD7;
    TEN_LOCATION(8) float4 BoneWeights   : TEXCOORD8;
    TEN_LOCATION(9) float4 Tangent       : TEXCOORD9;
    TEN_LOCATION(10) float4 FaceNormal   : TEXCOORD10;
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
    nointerpolation uint2  LightSlice   : TEXCOORD6;
    float3 WorldPosition                : TEXCOORD7;
    float3 Normal                       : TEXCOORD8;
    float3 Tangent                      : TEXCOORD9;
    float3 Binormal                     : TEXCOORD10;
    float3 FaceNormal                   : TEXCOORD11;
    float  Sheen                        : TEXCOORD12;
    float4 FogBulbs                     : TEXCOORD13;
};

float3 CombineLights(float3 ambient, float3 vertexLight, float3 position, Surface surface, float sheen, uint2 lightSlice, float fogDensity)
{
    float3 diffuse = 0.0f;
    float3 shadow = 0.0f;
    float3 specular = 0.0f;

    float3 normal = surface.Normal;
    uint count = min(lightSlice.y & LT_MASK, LIGHTS_PER_DRAW_MAX);
    for (uint i = 0; i < count; i++)
    {
        GpuLight light = Lights[lightSlice.x + i];

        if (light.Type == LT_SUN)
        {
            diffuse += DoDirectionalLight(normal, light);
            specular += DoSpecularSun(normal, CameraDirection, light, sheen, surface.Specular, surface.Roughness);
        }
        else if (light.Type == LT_POINT)
        {
            diffuse += DoPointLight(position, normal, light);
            specular += DoSpecularPoint(position, normal, CameraDirection, light, sheen, surface.Specular, surface.Roughness);
        }
        else if (light.Type == LT_SPOT)
        {
            diffuse += DoSpotLight(position, normal, light);
            specular += DoSpecularSpot(position, normal, CameraDirection, light, sheen, surface.Specular, surface.Roughness);
        }
        else if (light.Type == LT_SHADOW)
        {
            shadow += DoShadowLight(position, normal, light);
        }
    }

    shadow = saturate(shadow * COLOR_MODULATION_SCALE);
    float3 texel = surface.Color.rgb;
    float3 combined = (ambient - shadow) * texel + diffuse * COLOR_MODULATION_SCALE * texel + specular * COLOR_MODULATION_SCALE + surface.Emissive;
    return saturate((combined - fogDensity) * vertexLight);
}

PixelInput VS(VertexInput input)
{
    GpuObjectInstance instance = Instances[input.InstanceIndex];

    float4x4 world = instance.World;
    uint skinningMode = instance.Skinning >> 24;
    if (skinningMode != SKINNING_NONE)
        world = mul(BlendBones(Bones, instance.Skinning & 0xFFFFFF, input.BoneIndices, input.BoneWeights, skinningMode), instance.World);

    float wibble = Wibble(input.Effects, DecodeHash(input.AnimationData), InterpolatedFrame);
    float4 worldPosition = mul(float4(Move(input.Position, input.Effects, wibble), 1.0f), world);

    PixelInput output;
    output.Position = mul(worldPosition, ViewProjection);
    output.UV = input.UV;
    output.UVScroll = float3(0.0f, 0.0f, 0.0f);

    if (AnimatedSetIndex >= 0)
    {
        GpuAnimatedSet set = AnimatedSets[AnimatedSetIndex];
        if (set.Type == ANIM_TYPE_LEGACY_WATERFALL)
        {
            // Legacy CalculateUVRotateForLegacyWaterfalls, affine in V so it is evaluated per vertex.
            GpuAnimatedFrame frame = AnimatedFrames[set.FirstFrame];
            float top = frame.TopLeft.y;
            float height = (frame.BottomLeft.y - top) / 2.0f;
            float relative = 1.0f - fmod(InterpolatedFrame, (float)set.Fps) / (float)set.Fps;
            output.UV.y = top + (input.UV.y - top) / 2.0f + height * relative;
        }
        else if (set.Type == ANIM_TYPE_UVROTATE)
        {
            output.UVScroll = GetUVRotateScroll(AnimatedFrames[set.FirstFrame], set, InterpolatedFrame);
        }
        else
        {
            uint frameIndex = GetAnimatedFrameIndex(set, Frame, DecodeAnimationFrameOffset(input.AnimationData));
            output.UV = GetFrameCorner(AnimatedFrames[frameIndex], DecodeIndexInPoly(input.Effects));
        }
    }

    output.Color = float4(Glow(input.Color.rgb, input.Effects, wibble) * instance.Color.rgb, input.Color.a * instance.Color.a);
    output.AmbientLight = instance.AmbientLight.rgb;
    output.LightMode = instance.LightMode;
    output.LightSlice = uint2(instance.LightOffset, instance.LightCount);
    output.WorldPosition = worldPosition.xyz;
    output.Normal = normalize(mul(input.Normal.xyz, (float3x3)world));
    output.Tangent = normalize(mul(input.Tangent.xyz, (float3x3)world));
    output.Binormal = mul(cross(input.Normal.xyz, input.Tangent.xyz), (float3x3)world);
    output.Binormal *= rsqrt(max(dot(output.Binormal, output.Binormal), 1e-8f));
    output.FaceNormal = normalize(mul(input.FaceNormal.xyz, (float3x3)world));
    output.Sheen = DecodeSheen(input.Effects);
    output.FogBulbs = GetFogBulbs(worldPosition.xyz, CameraPosition, false);
    output.Fog = GetDistanceFog(worldPosition.xyz, CameraPosition, FogMinDistance, FogMaxDistance);
    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    GpuMaterial material = Materials[MaterialIndex];
    Surface surface = SampleSurface(material, ApplyUVScroll(input.UV, input.UVScroll), AlphaThreshold, input.WorldPosition, PixelCameraPosition,
                                    input.Normal, input.Tangent, input.Binormal, input.FaceNormal);
    surface.Color.rgb = ApplyReflections(material, surface.Color.rgb, input.WorldPosition, PixelCameraPosition, surface.Normal,
                                         surface.Specular, View, InverseViewportSize.y / InverseViewportSize.x);

    float3 vertexLight = input.Color.rgb * COLOR_MODULATION_SCALE;
    float3 color = (input.LightMode == LIGHT_MODE_DYNAMIC) ?
        CombineLights(input.AmbientLight * COLOR_MODULATION_SCALE, vertexLight, input.WorldPosition, surface, input.Sheen, input.LightSlice, input.FogBulbs.w) :
        saturate(surface.Color.rgb * vertexLight + surface.Emissive - input.FogBulbs.w);

    // Only instances that do not cast shadows receive them, as in the legacy renderer.
    if (input.LightSlice.y & SHADOWABLE_MASK)
    {
        color = DoShadow(input.WorldPosition, surface.Normal, color, -0.5f, BlendMode);
        color = DoBlobShadows(input.WorldPosition, color);
    }

    float occlusion = surface.Occlusion * GetAmbientOcclusion(material, input.Position.xy * InverseViewportSize, AmbientOcclusionExponent, BlendMode, surface.Color.a);
    float4 result = ApplyFogBulbs(float4(saturate(color * occlusion), surface.Color.a), input.FogBulbs.rgb, BlendMode);
    result = ApplyDistanceFog(result, FogColor, input.Fog, BlendMode);
    result.a *= input.Color.a;
    if (BlendMode == BLENDMODE_DISTORTION)
        return EncodeDistortionPayload(result, dot(input.WorldPosition - PixelCameraPosition, CameraDirection), 1.0f);

    return result;
}

PrepassOutput PSDepthNormal(PixelInput input)
{
    float3 emissive;
    float3 normal = SampleSurfaceNormal(Materials[MaterialIndex], ApplyUVScroll(input.UV, input.UVScroll), AlphaThreshold, input.WorldPosition,
                                        PixelCameraPosition, input.Normal, input.Tangent, input.Binormal, input.FaceNormal, emissive);
    return GetPrepassOutput(EncodeViewNormal(normal, View), emissive, input.Fog);
}

// Objects of the 2D layer (inventory, pickups, display items), as the legacy Inventory shader: the instance color
// replaces the vertex color, lit by a fixed sun tinted by the ambient light, without scene lights and fog.
PixelInput VSInventory(VertexInput input)
{
    PixelInput output = VS(input);
    output.Color = Instances[input.InstanceIndex].Color;
    return output;
}

struct InventoryOutput
{
    float4 Color    : SV_Target0;
    float4 Emissive : SV_Target1; // Source of the menu glow.
};

InventoryOutput PSInventory(PixelInput input)
{
    float2 uv = ApplyUVScroll(input.UV, input.UVScroll);
    float4 texel = ColorMap.Sample(ColorMapSampler, uv);
    float4 color = float4(texel.rgb * input.Color.rgb * COLOR_MODULATION_SCALE, texel.a * input.Color.a);
    ApplyAlphaTest(color.a, AlphaThreshold);

    float4 orsh = ORSHMap.Sample(ORSHMapSampler, uv);
    float3 emissive = EmissiveMap.Sample(EmissiveMapSampler, uv).rgb * GetEmissiveIntensity(Materials[MaterialIndex]);
    float3x3 tangentFrame = float3x3(input.Tangent, input.Binormal, input.Normal);
    float3 normal = normalize(mul(UnpackNormalMap(NormalMap.Sample(NormalMapSampler, uv)), tangentFrame));

    GpuLight light = (GpuLight)0;
    light.Type = LT_SUN;
    light.Intensity = 0.3f;
    light.Direction = normalize(float3(-1.0f, -0.707f, -0.5f));
    light.Color = input.AmbientLight * COLOR_MODULATION_SCALE;

    float3 lighting = DoDirectionalLight(normal, light) + DoSpecularSun(normal, CameraDirection, light, input.Sheen, orsh.z, orsh.y) + emissive;
    color.rgb = saturate(color.rgb + lighting * color.a);
    color.rgb = saturate(pow(color.rgb, 1.0f / max(Gamma, 0.01f)));

    InventoryOutput output;
    output.Color = color;
    output.Emissive = float4(emissive, 1.0f);
    return output;
}

// Shadow map pass: writes the light space depth; the alpha test keeps cut-out geometry out of the shadow.
float PSShadow(PixelInput input) : SV_TARGET
{
    clip(ColorMap.Sample(ColorMapSampler, ApplyUVScroll(input.UV, input.UVScroll)).a - AlphaThreshold);
    return input.Position.z;
}
