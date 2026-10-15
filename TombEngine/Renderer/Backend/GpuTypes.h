#pragma once

// C++ side of the structures and uniform blocks shared with the shaders in Shaders/RHI. The layouts themselves are
// defined once in Shaders/GpuShared.hlsli; sizes are checked here against the HLSL packing rules.

namespace TEN::Renderer::Backend
{
    using namespace TEN::Math::Library;

    // HLSL type names used by the shared definitions.
    using float2   = Vector2;
    using float3   = Vector3;
    using float4   = Vector4;
    using float4x4 = Matrix;
    using uint     = unsigned int;

    #include "Shaders/GpuShared.hlsli"

    constexpr auto ALPHA_TEST_THRESHOLD       = 0.5f;
    constexpr auto INSTANCE_INDEX_COUNT       = 65536;
    constexpr auto SSAO_KERNEL_SIZE           = 16; // Matches SSAO.hlsl; the bilateral blur hides the noise of fewer samples.
    constexpr auto LENS_FLARES_MAX            = 8;
    constexpr auto ANIM_TYPE_LEGACY_WATERFALL = 100; // Matches Common.hlsli.
    constexpr auto SHADOWABLE_MASK            = 1u << 16; // Light count flag of instances receiving shadows.

    // Skinning modes of GpuObjectInstance.Skinning, matching Skinning.hlsli.
    constexpr auto SKINNING_NONE    = 0;
    constexpr auto SKINNING_RIGID   = 1;
    constexpr auto SKINNING_CLASSIC = 3;
    constexpr auto SKINNING_FULL    = 2;

    // Level vertices packed at load from the 52 byte level vertex: room vertices take 40 bytes, object vertices add the
    // bone indices and weights. UVs are unorm16 atlas coordinates. Field names follow the level vertex.
    struct GpuRoomVertex
    {
        Vector3        Position                      = Vector3::Zero;
        unsigned int   Normal                        = 0;
        unsigned short UV[2]                         = {};
        unsigned int   Color                         = 0;
        unsigned int   Tangent                       = 0;
        unsigned int   FaceNormal                    = 0;
        unsigned int   Effects                       = 0;
        unsigned int   AnimationFrameOffsetIndexHash = 0;
    };

    struct GpuObjectVertex
    {
        Vector3        Position                      = Vector3::Zero;
        unsigned int   Normal                        = 0;
        unsigned short UV[2]                         = {};
        unsigned int   Color                         = 0;
        unsigned int   Tangent                       = 0;
        unsigned int   FaceNormal                    = 0;
        unsigned int   Effects                       = 0;
        unsigned int   AnimationFrameOffsetIndexHash = 0;
        unsigned char  BoneIndex[4]                  = {};
        unsigned char  BoneWeight[4]                 = {};
    };

    static_assert(sizeof(GpuRoomVertex) == 40, "GpuRoomVertex size.");
    static_assert(sizeof(GpuObjectVertex) == 48, "GpuObjectVertex size.");

    struct FrameVertexUniforms { TEN_FRAME_VERTEX_UNIFORMS };
    struct DrawVertexUniforms  { TEN_DRAW_VERTEX_UNIFORMS };
    struct SkyVertexUniforms   { TEN_SKY_VERTEX_UNIFORMS };
    struct SkyPixelUniforms    { TEN_SKY_PIXEL_UNIFORMS };
    struct FogBulbUniforms     { TEN_FOG_BULB_UNIFORMS };
    struct FramePixelUniforms  { TEN_FRAME_PIXEL_UNIFORMS };
    struct DrawPixelUniforms   { TEN_DRAW_PIXEL_UNIFORMS };
    struct GpuShadowUniforms   { TEN_SHADOW_UNIFORMS };
    struct OverlayUniforms     { TEN_OVERLAY_UNIFORMS };

    static_assert(sizeof(GpuAnimatedFrame) == 32, "GpuAnimatedFrame size.");
    static_assert(sizeof(GpuAnimatedSet) == 32, "GpuAnimatedSet size.");
    static_assert(sizeof(GpuObjectInstance) == 112, "GpuObjectInstance size.");
    static_assert(sizeof(GpuLight) == 64, "GpuLight size.");
    static_assert(sizeof(GpuMaterial) == 80, "GpuMaterial size.");
    static_assert(sizeof(GpuDecal) == 32, "GpuDecal size.");
    static_assert(sizeof(GpuSprite) == 96, "GpuSprite size.");
    static_assert(sizeof(GpuFogBulb) == 64, "GpuFogBulb size.");

    // Uniform blocks are multiples of 16 bytes with no member crossing a 16 byte boundary.
    static_assert(sizeof(FrameVertexUniforms) == 96, "FrameVertexUniforms size.");
    static_assert(sizeof(DrawVertexUniforms) == 16, "DrawVertexUniforms size.");
    static_assert(sizeof(SkyVertexUniforms) == 160, "SkyVertexUniforms size.");
    static_assert(sizeof(SkyPixelUniforms) == 32, "SkyPixelUniforms size.");
    static_assert(sizeof(FogBulbUniforms) == 2064, "FogBulbUniforms size.");
    static_assert(sizeof(FramePixelUniforms) == 144, "FramePixelUniforms size.");
    static_assert(sizeof(DrawPixelUniforms) == 48, "DrawPixelUniforms size.");
    static_assert(sizeof(GpuShadowUniforms) == 720, "GpuShadowUniforms size.");
    static_assert(sizeof(OverlayUniforms) == 16, "OverlayUniforms size.");

    static_assert(offsetof(FramePixelUniforms, View) == 48, "FramePixelUniforms.View must start a register.");
    static_assert(offsetof(SkyVertexUniforms, ParaboloidView) == 80, "SkyVertexUniforms.ParaboloidView must start a register.");
    static_assert(offsetof(FogBulbUniforms, FogBulbs) == 16, "FogBulbUniforms.FogBulbs must start a register.");
    static_assert(offsetof(GpuShadowUniforms, LightViewProjections) == 64, "GpuShadowUniforms.LightViewProjections must start a register.");
    static_assert(offsetof(GpuShadowUniforms, Spheres) == 464, "GpuShadowUniforms.Spheres must start a register.");
}
