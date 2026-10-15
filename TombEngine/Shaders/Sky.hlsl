// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=2 uniform_buffers=4
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSHorizonMask samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Classic sky layers and horizon meshes: texture tinted by the layer color, no lighting or depth. The paraboloid
// mode draws them into one hemisphere of the dual paraboloid map of skybox reflective materials.

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
    TEN_SKY_PIXEL_UNIFORMS
};

TEN_VS_STORAGE_BUFFER(GpuAnimatedFrame, AnimatedFrames, 0);
TEN_VS_STORAGE_BUFFER(GpuAnimatedSet, AnimatedSets, 1);

TEN_PS_TEXTURE(Texture2DArray, ColorMap, 0); // Sky texture or horizon page; TextureLayer selects the layer.

#define HORIZON_MASK_ALPHA_THRESHOLD 0.5f

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
    float  RelativeY                : TEXCOORD4; // World Y relative to the camera, for the horizon gradients.
};

PixelInput VS(VertexInput input)
{
    float3 worldPosition = mul(float4(input.Position, 1.0f), World).xyz;

    PixelInput output;
    output.Position = mul(float4(worldPosition, 1.0f), ViewProjection);
    output.ClipDepth = 1.0f;
    output.RelativeY = worldPosition.y - CameraPosition.y;

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

// Horizon gradient: alpha ramp over the mesh height, t being 0 where the mesh is fully hidden.
float GetHorizonGradient(float t, float amount)
{
    float fadeExtent = max(amount * 0.75f, 0.001f);
    float gradientAlpha = smoothstep(0.0f, 1.0f, pow(saturate(t / fadeExtent), 2.2f));
    return lerp(1.0f, gradientAlpha, saturate(amount * 2.0f));
}

// Fade hides the horizon mesh from its top down, rise from its bottom up (TEN Y points down).
float GetHorizonGradientAlpha(float relativeY)
{
    if (MeshWorldYRange <= 0.0f)
        return 1.0f;

    float t = saturate((relativeY - MeshWorldYMin) / MeshWorldYRange);
    float alpha = 1.0f;
    if (HorizonGradientFade > 0.0f)
        alpha *= GetHorizonGradient(t, HorizonGradientFade);

    if (HorizonGradientRise > 0.0f)
        alpha *= GetHorizonGradient(1.0f - t, HorizonGradientRise);

    return alpha;
}

float4 PS(PixelInput input) : SV_TARGET
{
    clip(input.ClipDepth);

    float4 color = ColorMap.Sample(ColorMapSampler, float3(ApplyUVScroll(input.UV, input.UVScroll), TextureLayer));
    float3 light = saturate(SkyColor.rgb - input.FogBulbs.w * 1.4f);
    return float4(color.rgb * light + saturate(input.FogBulbs.rgb), color.a * SkyColor.a * GetHorizonGradientAlpha(input.RelativeY));
}

// Horizon silhouette for the god rays: white where the texel passes the alpha test.
float4 PSHorizonMask(PixelInput input) : SV_TARGET
{
    float4 color = ColorMap.Sample(ColorMapSampler, float3(ApplyUVScroll(input.UV, input.UVScroll), TextureLayer));
    clip(color.a - HORIZON_MASK_ALPHA_THRESHOLD);
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
