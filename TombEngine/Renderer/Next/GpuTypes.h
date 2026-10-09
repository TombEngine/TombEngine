#pragma once

// CPU mirrors of the structures and uniform blocks used by the shaders in Shaders/RHI.

namespace TEN::Renderer::Next
{
    using namespace DirectX::SimpleMath;

    constexpr auto ALPHA_TEST_THRESHOLD = 0.5f;
    constexpr auto INSTANCE_INDEX_COUNT = 65536;
    constexpr auto SSAO_KERNEL_SIZE     = 32;
    constexpr auto SHADOW_SPHERES_MAX   = 16;
    constexpr auto FOG_BULBS_MAX        = 32;
    constexpr auto SHADOWABLE_MASK      = 1u << 16; // Light count flag of instances receiving shadows.

    // Uniform layouts mirror the cbuffers in Shaders/RHI/Rooms.hlsl.
    struct FrameVertexUniforms
    {
        Matrix       ViewProjection    = Matrix::Identity;
        Vector3      CameraPosition    = Vector3::Zero;
        float        FogMinDistance    = 0.0f;
        float        FogMaxDistance    = 0.0f;
        unsigned int Frame             = 0;
        float        InterpolatedFrame = 0.0f;
        unsigned int CameraUnderwater  = 0;
    };

    struct DrawVertexUniforms
    {
        int          AnimatedSetIndex = NO_VALUE;
        unsigned int IsWaterRoom      = 0;
        float        Padding[2]       = {};
    };

    struct FramePixelUniforms
    {
        Vector4 FogColor                 = Vector4::Zero;
        Vector3 CameraPosition           = Vector3::Zero;
        float   AmbientOcclusionExponent = 0.0f;
        Vector3 CameraDirection          = Vector3::Zero;
        float   Padding0                 = 0.0f;
        Matrix  View                     = Matrix::Identity;
        Vector2 InverseViewportSize      = Vector2::Zero;
        Vector2 CausticsStartUV          = Vector2::Zero;
        Vector2 CausticsSize             = Vector2::Zero;
        Vector2 Padding1                 = Vector2::Zero;
    };

    static_assert(sizeof(FramePixelUniforms) == 144, "FramePixelUniforms must match FrameUniforms.hlsli.");

    struct DrawPixelUniforms
    {
        float        AlphaThreshold = 0.0f;
        unsigned int BlendMode      = 0;
        unsigned int LightOffset    = 0;
        unsigned int LightCount     = 0;
        unsigned int MaterialIndex  = 0;
        unsigned int IsCausticsRoom = 0;
        unsigned int DecalOffset    = 0;
        unsigned int DecalCount     = 0;
    };

    // Same layout as GpuDecal in Decals.hlsli.
    struct GpuDecal
    {
        Vector3 Position   = Vector3::Zero;
        float   Radius     = 0.0f;
        float   Opacity    = 0.0f;
        int     Pattern    = 0;
        Vector2 Padding    = Vector2::Zero;
    };

    static_assert(sizeof(GpuDecal) == 32, "GpuDecal must match Decals.hlsli.");

    // Same layout as GpuFogBulb in FogBulbs.hlsli.
    struct GpuFogBulb
    {
        Vector3 Position                       = Vector3::Zero;
        float   Density                        = 0.0f;
        Vector3 Color                          = Vector3::Zero;
        float   SquaredRadius                  = 0.0f;
        Vector3 FogBulbToCameraVector          = Vector3::Zero;
        float   SquaredCameraToFogBulbDistance = 0.0f;
        Vector4 Padding                        = Vector4::Zero;
    };

    struct FogBulbUniforms
    {
        int        Count      = 0;
        int        Padding[3] = {};
        GpuFogBulb FogBulbs[FOG_BULBS_MAX] = {};
    };

    static_assert(sizeof(FogBulbUniforms) == 2064, "FogBulbUniforms must match FogBulbs.hlsli.");

    // Same layout as SkyVertexUniforms in Sky.hlsl.
    struct SkyVertexUniforms
    {
        Matrix       World             = Matrix::Identity;
        unsigned int IsFogBulbsEnabled = 0;
        unsigned int Padding[3]        = {};
    };

    // Same layout as GpuMaterial in Materials.hlsli; properties are interpolated every frame.
    struct GpuMaterial
    {
        Vector4      Properties[4] = {};
        unsigned int TypeAndFlags  = 0;
        unsigned int Padding[3]    = {};
    };

    static_assert(sizeof(GpuMaterial) == 80, "GpuMaterial must match Materials.hlsli.");

    // Same layout as the legacy ShaderLight and GpuLight in Lighting.hlsli.
    struct GpuLight
    {
        Vector3      Position  = Vector3::Zero;
        unsigned int Type      = 0;
        Vector3      Color     = Vector3::Zero;
        float        Intensity = 0.0f;
        Vector3      Direction = Vector3::Zero;
        float        In        = 0.0f;
        float        Out       = 0.0f;
        float        InRange   = 0.0f;
        float        OutRange  = 0.0f;
        float        Padding   = 0.0f;
    };

    static_assert(sizeof(GpuLight) == 64, "GpuLight must match Lighting.hlsli.");

    // Same layout as ShadowUniforms in Shadows.hlsli.
    struct GpuShadowUniforms
    {
        GpuLight Light                   = {};
        Matrix   LightViewProjections[6] = {};
        int      CastShadows             = 0;
        int      SphereCount             = 0;
        int      ShadowMapSize           = 1;
        int      Padding                 = 0;
        Vector4  Spheres[SHADOW_SPHERES_MAX] = {};
    };

    static_assert(sizeof(GpuShadowUniforms) == 720, "GpuShadowUniforms must match Shadows.hlsli.");

    struct GpuAnimatedFrame
    {
        Vector2 Corners[4];
    };

    struct GpuAnimatedSet
    {
        unsigned int FirstFrame        = 0;
        unsigned int NumFrames         = 0;
        unsigned int Fps               = 0;
        unsigned int Type              = 0;
        float        UVRotateDirection = 0.0f;
        float        UVRotateSpeed     = 0.0f;
        float        Padding[2]        = {};
    };

    struct GpuObjectInstance
    {
        Matrix       World        = Matrix::Identity;
        Vector4      Color        = Vector4::One;
        Vector4      AmbientLight = Vector4::One;
        unsigned int LightMode    = 0;
        unsigned int LightOffset  = 0;
        unsigned int LightCount   = 0;
        unsigned int Skinning     = 0;
    };

    // Skinning modes of ObjectInstance.Skinning, matching Skinning.hlsli.
    constexpr auto SKINNING_NONE    = 0;
    constexpr auto SKINNING_RIGID   = 1;
    constexpr auto SKINNING_CLASSIC = 3;
    constexpr auto SKINNING_FULL    = 2;

    static_assert(sizeof(GpuObjectInstance) == 112, "GpuObjectInstance must match ObjectInstance in Common.hlsli.");
    static_assert(sizeof(FrameVertexUniforms) == 96, "FrameVertexUniforms must match Rooms.hlsl.");
    static_assert(sizeof(GpuAnimatedFrame) == 32, "GpuAnimatedFrame must match Common.hlsli.");
    static_assert(sizeof(GpuAnimatedSet) == 32, "GpuAnimatedSet must match Common.hlsli.");

    // Sprite corners expanded to world space, with a packed RGBA8 color.
    struct GpuSpriteCorner
    {
        Vector3      Position = Vector3::Zero;
        unsigned int Color    = 0;
    };

    struct GpuSprite
    {
        GpuSpriteCorner Corners[4] = {};
        Vector4         UVX        = Vector4::Zero;
        Vector4         UVY        = Vector4::Zero;
    };

    static_assert(sizeof(GpuSprite) == 96, "GpuSprite must match SpriteInstance in Sprites.hlsl.");
}
