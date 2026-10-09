// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=1 uniform_buffers=4
// TEN_SHADER: Pixel PS samplers=2 storage_textures=0 storage_buffers=0 uniform_buffers=4

// Billboards and 3D quads share one path: corners are expanded to world space on the CPU, each sprite
// is one instance of six vertices, and the corner index comes from a static vertex stream.

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
    float SpritePadding0;
    float SpritePadding1;
    float SpritePadding2;
};

TEN_VS_STORAGE_BUFFER(GpuSprite, Sprites, 0);

TEN_PS_TEXTURE(Texture2D, ColorMap, 0);
TEN_PS_TEXTURE(Texture2D, SceneDepthMap, 1); // Hardware depth of the opaque geometry, copied after the prepass.

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
};

float4 UnpackColor(uint packedColor)
{
    return float4(packedColor & 0xFF, (packedColor >> 8) & 0xFF, (packedColor >> 16) & 0xFF, packedColor >> 24) / 255.0f;
}

PixelInput VS(VertexInput input)
{
    GpuSprite sprite = Sprites[input.InstanceIndex];
    GpuSpriteCorner corner = sprite.Corners[input.Corner];

    PixelInput output;
    output.Position = mul(float4(corner.Position, 1.0f), ViewProjection);
    output.ClipPosition = output.Position;
    output.UV = float2(sprite.UVX[input.Corner], sprite.UVY[input.Corner]);
    output.Color = UnpackColor(corner.Color);
    output.Fog = GetDistanceFog(corner.Position, CameraPosition, FogMinDistance, FogMaxDistance);
    output.FogBulbs = GetFogBulbs(corner.Position, CameraPosition, false).rgb;
    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    float4 texel = ColorMap.Sample(ColorMapSampler, input.UV) * input.Color;
    float4 color = texel;
    clip(color.a - AlphaThreshold);

    // Soft particles fade where they meet the opaque geometry, as in the legacy InstancedSprites shader.
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
