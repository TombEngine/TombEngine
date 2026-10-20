#ifndef TEN_GPU_SHARED
#define TEN_GPU_SHARED

// GPU structures and uniform blocks shared by the shaders and the C++ renderer (Renderer/Backend/GpuTypes.h includes
// this file inside its namespace, with HLSL type names aliased to SimpleMath types). Uniform blocks are field lists:
// HLSL declares them as cbuffer members, C++ as struct members. TEN_DEFAULT keeps C++ default values.
// Packing is explicit: a float3 is always followed by a scalar, and arrays in uniform blocks hold 16 byte elements.

#ifdef __cplusplus
#define TEN_DEFAULT(value) = value

constexpr auto FOG_BULBS_MAX      = 32;
constexpr auto SHADOW_SPHERES_MAX = 16;

constexpr auto LIGHT_CLUSTER_TILES_X    = 16;
constexpr auto LIGHT_CLUSTER_TILES_Y    = 9;
constexpr auto LIGHT_CLUSTER_SLICES     = 24;
constexpr auto LIGHT_CLUSTER_LIGHTS_MAX = 64;
#else
#define TEN_DEFAULT(value)

#define FOG_BULBS_MAX      32
#define SHADOW_SPHERES_MAX 16

#define LIGHT_CLUSTER_TILES_X    16
#define LIGHT_CLUSTER_TILES_Y    9
#define LIGHT_CLUSTER_SLICES     24
#define LIGHT_CLUSTER_LIGHTS_MAX 64
#endif

// Storage buffer elements.

// Corners of one animated texture frame, in polygon corner order.
struct GpuAnimatedFrame
{
    float2 TopLeft     TEN_DEFAULT(Vector2::Zero);
    float2 TopRight    TEN_DEFAULT(Vector2::Zero);
    float2 BottomRight TEN_DEFAULT(Vector2::Zero);
    float2 BottomLeft  TEN_DEFAULT(Vector2::Zero);
};

// Animated texture sequence: a range of GpuAnimatedFrame entries played at Fps, or a UV rotation, by Type.
struct GpuAnimatedSet
{
    uint  FirstFrame        TEN_DEFAULT(0);
    uint  NumFrames         TEN_DEFAULT(0);
    uint  Fps               TEN_DEFAULT(0);
    uint  Type              TEN_DEFAULT(0);
    float UVRotateDirection TEN_DEFAULT(0.0f);
    float UVRotateSpeed     TEN_DEFAULT(0.0f);
    float Padding0          TEN_DEFAULT(0.0f);
    float Padding1          TEN_DEFAULT(0.0f);
};

// Instances of outdoor rooms write the outdoor mask of the dust storm in the prepass.
#define INSTANCE_LIGHT_MODE_MASK 0xFF
#define INSTANCE_OUTDOOR_FLAG    0x100
// Instances lit by the dynamic lights of the view through the light clusters; their light slice holds static lights only.
#define INSTANCE_CLUSTER_LIGHTS_FLAG 0x200

// Per-instance data of statics and moveable meshes, read through the instance index stream.
struct GpuObjectInstance
{
    float4x4 World        TEN_DEFAULT(Matrix::Identity);
    float4   Color        TEN_DEFAULT(Vector4::One);
    float4   AmbientLight TEN_DEFAULT(Vector4::One);
    uint     LightMode    TEN_DEFAULT(0); // Light mode in the low 8 bits, INSTANCE_OUTDOOR_FLAG for outdoor rooms.
    uint     LightOffset  TEN_DEFAULT(0);
    uint     LightCount   TEN_DEFAULT(0); // Light count in the low 16 bits, light type mask in the high bits.
    uint     Skinning     TEN_DEFAULT(0); // Bone palette offset in the low 24 bits, skinning mode in the high 8 bits.
};

// Element of the frame light buffer, filled by SceneRenderer::ToGpuLight. Type is an LT_* value of Lighting.hlsli;
// spot ranges are stored as cosines.
struct GpuLight
{
    float3 Position  TEN_DEFAULT(Vector3::Zero);
    uint   Type      TEN_DEFAULT(0);
    float3 Color     TEN_DEFAULT(Vector3::Zero);
    float  Intensity TEN_DEFAULT(0.0f);
    float3 Direction TEN_DEFAULT(Vector3::Zero);
    float  In        TEN_DEFAULT(0.0f);
    float  Out       TEN_DEFAULT(0.0f);
    float  InRange   TEN_DEFAULT(0.0f);
    float  OutRange  TEN_DEFAULT(0.0f);
    float  Padding   TEN_DEFAULT(0.0f);
};

// Material properties, interpolated every frame. Scalars pad the end: a uint3 at offset 68 breaks the Vulkan buffer layout rules.
struct GpuMaterial
{
    float4 Properties[4];
    uint   TypeAndFlags TEN_DEFAULT(0);
    uint   Padding0     TEN_DEFAULT(0);
    uint   Padding1     TEN_DEFAULT(0);
    uint   Padding2     TEN_DEFAULT(0);
};

struct GpuDecal
{
    float3 Position TEN_DEFAULT(Vector3::Zero);
    float  Radius   TEN_DEFAULT(0.0f);
    float  Opacity  TEN_DEFAULT(0.0f);
    int    Pattern  TEN_DEFAULT(0);
    float2 Padding  TEN_DEFAULT(Vector2::Zero);
};

// Colors are real uint fields: integer bits stored in float components can be altered by drivers
// that canonicalize NaN patterns (opaque RGBA8 colors are NaN when read as float).
struct GpuSpriteCorner
{
    float3 Position TEN_DEFAULT(Vector3::Zero);
    uint   Color    TEN_DEFAULT(0); // Packed RGBA8.
};

// Sprite corners expanded to world space.
struct GpuSprite
{
    GpuSpriteCorner Corners[4];
    float4          UVX TEN_DEFAULT(Vector4::Zero);
    float4          UVY TEN_DEFAULT(Vector4::Zero);
};

struct GpuFogBulb
{
    float3 Position                       TEN_DEFAULT(Vector3::Zero);
    float  Density                        TEN_DEFAULT(0.0f);
    float3 Color                          TEN_DEFAULT(Vector3::Zero);
    float  SquaredRadius                  TEN_DEFAULT(0.0f);
    float3 FogBulbToCameraVector          TEN_DEFAULT(Vector3::Zero);
    float  SquaredCameraToFogBulbDistance TEN_DEFAULT(0.0f);
    float4 Padding                        TEN_DEFAULT(Vector4::Zero);
};

// Uniform blocks. Field names are unique across blocks: HLSL cbuffer members are globals.

// Vertex slot 0, pushed once per frame.
#define TEN_FRAME_VERTEX_UNIFORMS \
    float4x4 ViewProjection    TEN_DEFAULT(Matrix::Identity); \
    float3   CameraPosition    TEN_DEFAULT(Vector3::Zero); \
    float    FogMinDistance    TEN_DEFAULT(0.0f); \
    float    FogMaxDistance    TEN_DEFAULT(0.0f); \
    uint     Frame             TEN_DEFAULT(0); \
    float    InterpolatedFrame TEN_DEFAULT(0.0f); \
    uint     CameraUnderwater  TEN_DEFAULT(0);

// Vertex slot 1, per draw. IsWaterRoom: water rooms seen from outside the water are refracted.
#define TEN_DRAW_VERTEX_UNIFORMS \
    int    AnimatedSetIndex TEN_DEFAULT(NO_VALUE); \
    uint   IsWaterRoom      TEN_DEFAULT(0); \
    float2 DrawPadding      TEN_DEFAULT(Vector2::Zero);

// Vertex slot 2 of the sky shaders. Hemisphere: -1 front, 1 back.
#define TEN_SKY_VERTEX_UNIFORMS \
    float4x4 World             TEN_DEFAULT(Matrix::Identity); \
    uint     IsFogBulbsEnabled TEN_DEFAULT(0); \
    uint     IsParaboloid      TEN_DEFAULT(0); \
    float    Hemisphere        TEN_DEFAULT(0.0f); \
    float    SkyPadding0       TEN_DEFAULT(0.0f); \
    float4x4 ParaboloidView    TEN_DEFAULT(Matrix::Identity); \
    float    ParaboloidNear    TEN_DEFAULT(1.0f); \
    float    ParaboloidFar     TEN_DEFAULT(1.0f); \
    float    SkyPadding1       TEN_DEFAULT(0.0f); \
    float    SkyPadding2       TEN_DEFAULT(0.0f);

// Pixel slot 2 of the sky shaders: layer or horizon color, and the gradient fades of the horizons.
#define TEN_SKY_PIXEL_UNIFORMS \
    float4 SkyColor            TEN_DEFAULT(Vector4::One); \
    float  HorizonGradientFade TEN_DEFAULT(0.0f); \
    float  HorizonGradientRise TEN_DEFAULT(0.0f); \
    float  MeshWorldYMin       TEN_DEFAULT(0.0f); \
    float  MeshWorldYRange     TEN_DEFAULT(1.0f);

// Vertex slot 3.
#define TEN_FOG_BULB_UNIFORMS \
    int        FogBulbCount    TEN_DEFAULT(0); \
    int        FogBulbPadding0 TEN_DEFAULT(0); \
    int        FogBulbPadding1 TEN_DEFAULT(0); \
    int        FogBulbPadding2 TEN_DEFAULT(0); \
    GpuFogBulb FogBulbs[FOG_BULBS_MAX];

// Pixel slot 0, pushed once per frame. AmbientOcclusionExponent is zero when ambient occlusion is disabled.
// Gamma applies to overlay objects only: the scene applies gamma in the post-process.
// CausticsStartUV and CausticsSize: current caustics frame in its sprite atlas page.
// ClusterViewProjection, ClusterNearDepth and ClusterSliceScale place the room light clusters; a zero scale means none.
// ClusterLightOffset and ClusterLightCount: the dynamic light slice of the view in the light buffer, which the cluster
// masks index.
#define TEN_FRAME_PIXEL_UNIFORMS \
    float4   FogColor                 TEN_DEFAULT(Vector4::Zero); \
    float3   PixelCameraPosition      TEN_DEFAULT(Vector3::Zero); \
    float    AmbientOcclusionExponent TEN_DEFAULT(0.0f); \
    float3   CameraDirection          TEN_DEFAULT(Vector3::Zero); \
    float    Gamma                    TEN_DEFAULT(1.0f); \
    float4x4 View                     TEN_DEFAULT(Matrix::Identity); \
    float2   InverseViewportSize      TEN_DEFAULT(Vector2::Zero); \
    float2   CausticsStartUV          TEN_DEFAULT(Vector2::Zero); \
    float2   CausticsSize             TEN_DEFAULT(Vector2::Zero); \
    float    ClusterNearDepth         TEN_DEFAULT(0.0f); \
    float    ClusterSliceScale        TEN_DEFAULT(0.0f); \
    float4x4 ClusterViewProjection    TEN_DEFAULT(Matrix::Identity); \
    uint     ClusterLightOffset       TEN_DEFAULT(0); \
    uint     ClusterLightCount        TEN_DEFAULT(0); \
    float2   ClusterPadding           TEN_DEFAULT(Vector2::Zero);

// Pixel slot 1, per draw. Light and decal slices and the caustics flag apply to rooms only.
// TextureLayer selects the page in the texture arrays of the bound material maps.
// LightCount: light count in the low 16 bits, light type mask in the high bits.
#define TEN_DRAW_PIXEL_UNIFORMS \
    float AlphaThreshold    TEN_DEFAULT(0.0f); \
    uint  BlendMode         TEN_DEFAULT(0); \
    uint  LightOffset       TEN_DEFAULT(0); \
    uint  LightCount        TEN_DEFAULT(0); \
    uint  MaterialIndex     TEN_DEFAULT(0); \
    uint  IsCausticsRoom    TEN_DEFAULT(0); \
    uint  DecalOffset       TEN_DEFAULT(0); \
    uint  DecalCount        TEN_DEFAULT(0); \
    uint  TextureLayer      TEN_DEFAULT(0); \
    uint  IsOutdoorRoom     TEN_DEFAULT(0); \
    uint  DrawPixelPadding1 TEN_DEFAULT(0); \
    uint  DrawPixelPadding2 TEN_DEFAULT(0);

// Pixel slot 2 of the scene shaders. Spheres: xyz center, w radius.
#define TEN_SHADOW_UNIFORMS \
    GpuLight ShadowLight; \
    float4x4 LightViewProjections[6]; \
    int      CastShadows   TEN_DEFAULT(0); \
    int      SphereCount   TEN_DEFAULT(0); \
    int      ShadowMapSize TEN_DEFAULT(1); \
    int      ShadowPadding TEN_DEFAULT(0); \
    float4   Spheres[SHADOW_SPHERES_MAX];

// Pixel slot 1 of the water surfaces. Absorption is per block of water, Ambient lights the water color and replaces
// the reflections that leave the screen indoors. Wave tile: world units of the wave normal map.
#define TEN_WATER_PIXEL_UNIFORMS \
    float4x4 WaterViewProjection     TEN_DEFAULT(Matrix::Identity); \
    float3   WaterColor              TEN_DEFAULT(Vector3::Zero); \
    float    WaterTime               TEN_DEFAULT(0.0f); \
    float3   WaterAbsorption         TEN_DEFAULT(Vector3::Zero); \
    float    WaterRefractionStrength TEN_DEFAULT(0.0f); \
    float3   WaterAmbient            TEN_DEFAULT(Vector3::Zero); \
    float    WaterReflectionStrength TEN_DEFAULT(1.0f); \
    float2   WaterFlowDirection      TEN_DEFAULT(Vector2::Zero); \
    float    WaterFlowSpeed          TEN_DEFAULT(0.0f); \
    float    WaterWaveTile           TEN_DEFAULT(1.0f); \
    float    WaterWaveStrength       TEN_DEFAULT(1.0f); \
    float    WaterSpecular           TEN_DEFAULT(0.0f); \
    float    WaterNearPlane          TEN_DEFAULT(1.0f); \
    float    WaterFarPlane           TEN_DEFAULT(1.0f); \
    uint     WaterLightCount         TEN_DEFAULT(0); \
    uint     IsWaterSkyReflected     TEN_DEFAULT(0); \
    float    WaterPadding0           TEN_DEFAULT(0.0f); \
    float    WaterPadding1           TEN_DEFAULT(0.0f);

// Pixel slot 2 of the underwater fog: the fog path ends at the height of the surface above the camera.
#define TEN_UNDERWATER_FOG_UNIFORMS \
    float4x4 UnderwaterInverseViewProjection TEN_DEFAULT(Matrix::Identity); \
    float    UnderwaterSurfaceHeight         TEN_DEFAULT(0.0f); \
    float    UnderwaterPadding0              TEN_DEFAULT(0.0f); \
    float    UnderwaterPadding1              TEN_DEFAULT(0.0f); \
    float    UnderwaterPadding2              TEN_DEFAULT(0.0f);

// Vertex slot 0 of the 2D layer.
#define TEN_OVERLAY_UNIFORMS \
    float2 InverseTargetSize TEN_DEFAULT(Vector2::Zero); \
    float2 OverlayPadding    TEN_DEFAULT(Vector2::Zero);

#endif // TEN_GPU_SHARED
