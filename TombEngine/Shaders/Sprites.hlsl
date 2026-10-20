// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=1 uniform_buffers=4
// TEN_SHADER: Pixel PS samplers=4 storage_textures=0 storage_buffers=2 uniform_buffers=4
// TEN_SHADER: Pixel PSSky samplers=4 storage_textures=0 storage_buffers=0 uniform_buffers=4

// Billboards and 3D quads share one path: corners are expanded to world space on the CPU, each sprite
// is one instance of six vertices, and the corner index comes from a static vertex stream (SceneRendererSprites.cpp).
// PS draws scene sprites (soft particles, laser effects, distortion, dynamic lights through the light clusters for
// lit sprites); PSSky draws stars and meteors. Both bind four textures, so the light buffers of PS follow them.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./FrameUniforms.hlsli"
#include "./FogBulbs.hlsli"
#include "./SpriteEffects.hlsli"

#define SPRITE_RENDER_DEFAULT       0
#define SPRITE_RENDER_LASER_BARRIER 1
#define SPRITE_RENDER_LASER_BEAM    2
#define LASER_FADE_FACTOR           0.789f

// Slot 3: slot 2 holds the shadow uniforms read by room and object draws interleaved with sorted sprites.
TEN_PS_UNIFORMS(SpriteUniforms, 3)
{
    uint  IsSoftParticle;
    uint  RenderType;
    float NearPlane;
    float FarPlane;
    float Time;
    float MoonDiskCosRadius; // Above 1 when no moon hides the stars.
    uint  IsLit;             // Dynamic lights of the view brighten the sprite.
    float SpritePadding1;
    float3 MoonDirection;
    float SpritePadding2;
};

TEN_VS_RAW_BUFFER(GpuSprite, Sprites, 0);

TEN_PS_TEXTURE(Texture2D, ColorMap, 0);
TEN_PS_TEXTURE(Texture2D, SceneDepthMap, 1); // Hardware depth of the opaque geometry, copied after the prepass.

// Cloud layers of the previous frame, which hide the stars and meteors.
TEN_PS_TEXTURE(Texture2D, CloudLayerA, 2);
TEN_PS_TEXTURE(Texture2D, CloudLayerB, 3);

TEN_PS_RAW_BUFFER(GpuLight, Lights, 4);
TEN_PS_RAW_BUFFER(uint2, LightClusters, 5);

#include "./Lighting.hlsli"
#include "./LightClusters.hlsli"

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
    float3 FogBulbs : TEXCOORD3;
    float4 ClipPosition : TEXCOORD4;
    float3 WorldDirection : TEXCOORD5; // From the camera, for the moon disc of the sky sprites.
    float3 WorldPosition : TEXCOORD6;
};

// Packed RGBA8, red in the low byte.
float4 UnpackColor(uint packedColor)
{
    return float4(packedColor & 0xFF, (packedColor >> 8) & 0xFF, (packedColor >> 16) & 0xFF, packedColor >> 24) / 255.0f;
}

PixelInput VS(VertexInput input)
{
    GpuSprite sprite = TEN_LOAD_RAW(GpuSprite, Sprites, input.InstanceIndex);
    GpuSpriteCorner corner = sprite.Corners[input.Corner];

    PixelInput output;
    output.Position = mul(float4(corner.Position, 1.0f), ViewProjection);
    output.ClipPosition = output.Position;
    output.WorldDirection = corner.Position - CameraPosition;
    output.WorldPosition = corner.Position;
    output.UV = float2(sprite.UVX[input.Corner], sprite.UVY[input.Corner]);
    output.Color = UnpackColor(corner.Color);
    output.Fog = GetDistanceFog(corner.Position, CameraPosition, FogMinDistance, FogMaxDistance);
    output.FogBulbs = GetFogBulbs(corner.Position, CameraPosition, false).rgb;
    return output;
}

// Shared sprite shading: alpha test, soft particle fade, laser effects, fog bulbs and distance fog, or the
// distortion payload for distortion sprites. Fog fades sprites to black, so additive sprites vanish in it.
float4 ShadeSprite(PixelInput input, float alphaScale)
{
    float4 texel = ColorMap.Sample(ColorMapSampler, input.UV) * input.Color;
    float4 color = texel;
    clip(color.a - AlphaThreshold);
    color.a *= alphaScale;

    // Soft particles fade where they meet the opaque geometry.
    if (IsSoftParticle)
    {
        float2 screenUV = input.ClipPosition.xy / input.ClipPosition.w * float2(0.5f, -0.5f) + 0.5f;
        float sceneDepth = LinearizeDepth(SceneDepthMap.SampleLevel(SceneDepthMapSampler, screenUV, 0.0f).x, NearPlane, FarPlane);
        float particleDepth = LinearizeDepth(input.ClipPosition.z / input.ClipPosition.w, NearPlane, FarPlane);

        if (particleDepth - sceneDepth > 0.01f)
            discard;

        color.a = min(color.a, (sceneDepth - particleDepth) * 1024.0f);
    }

    if (RenderType == SPRITE_RENDER_LASER_BARRIER)
        color = DoLaserBarrierEffect(input.Position.xyz, float4(texel.rgb * COLOR_MODULATION_SCALE, texel.a), input.UV, LASER_FADE_FACTOR, Time);
    else if (RenderType == SPRITE_RENDER_LASER_BEAM)
        color = DoLaserBeamEffect(input.Position.xyz, float4(texel.rgb * COLOR_MODULATION_SCALE, texel.a), input.UV, LASER_FADE_FACTOR, Time);

    color.rgb = saturate(color.rgb * (1.0f - Luma(input.FogBulbs)));
    color = ApplyDistanceFog(color, float4(0.0f, 0.0f, 0.0f, 0.0f), input.Fog, BlendMode);
    if (BlendMode == BLENDMODE_DISTORTION)
        return EncodeDistortionPayload(color, input.ClipPosition.w, 0.0f);

    return color;
}

// Light of a point or spot light reaching a position, without a surface normal: particles are lit from every side.
float3 GetSpriteLight(GpuLight light, float3 position)
{
    if (light.Type != LT_POINT && light.Type != LT_SPOT)
        return float3(0.0f, 0.0f, 0.0f);

    float3 lightVector = light.Position - position;
    float attenuation = saturate((light.Out - length(lightVector)) / max(light.Out - light.In, 1.0f));
    if (light.Type == LT_SPOT)
    {
        float cosine = dot(-normalize(lightVector), light.Direction);
        attenuation *= saturate((cosine - light.OutRange) / max(light.InRange - light.OutRange, 0.0001f));
    }

    return light.Color * light.Intensity * attenuation;
}

// Dynamic lights of the view reaching the light cluster of the position, or all of them outside the cluster grid.
float3 GetSpriteClusterLight(float3 position)
{
    float3 lighting = float3(0.0f, 0.0f, 0.0f);
    uint count = min(ClusterLightCount, LIGHTS_PER_DRAW_MAX);

    uint2 mask;
    if (count <= LIGHT_CLUSTER_LIGHTS_MAX && GetClusterLightMask(position, mask))
    {
        [loop]
        for (uint word = 0; word < 2; word++)
        {
            uint bits = (word == 0) ? mask.x : mask.y;

            [loop]
            while (bits != 0)
            {
                uint index = (word * 32) + firstbitlow(bits);
                bits &= bits - 1;
                lighting += GetSpriteLight(TEN_LOAD_RAW(GpuLight, Lights, ClusterLightOffset + index), position);
            }
        }
    }
    else
    {
        for (uint i = 0; i < count; i++)
            lighting += GetSpriteLight(TEN_LOAD_RAW(GpuLight, Lights, ClusterLightOffset + i), position);
    }

    return lighting;
}

// Lit sprites keep their color without dynamic lights and brighten near them, as smoke lit by a flare.
float4 PS(PixelInput input) : SV_TARGET
{
    float4 color = ShadeSprite(input, 1.0f);
    if (IsLit != 0)
        color.rgb = saturate(color.rgb * (1.0f + GetSpriteClusterLight(input.WorldPosition) * (1.0f - input.Fog)));

    return color;
}

// Stars and meteors: hidden inside the moon disc and faded by the cloud coverage.
float4 PSSky(PixelInput input) : SV_TARGET
{
    if (dot(normalize(input.WorldDirection), MoonDirection) > MoonDiskCosRadius)
        discard;

    float2 screenUV = input.ClipPosition.xy / input.ClipPosition.w * float2(0.5f, -0.5f) + 0.5f;
    float coverageA = CloudLayerA.Sample(CloudLayerASampler, screenUV).a;
    float coverageB = CloudLayerB.Sample(CloudLayerBSampler, screenUV).a;
    return ShadeSprite(input, 1.0f - saturate(coverageA + coverageB - coverageA * coverageB));
}
