#pragma once

#include "Math/Math.h"

#define SHAPE_RECTANGLE 0
#define SHAPE_TRIANGLE	1

// TODO: refactor AddString() functions for accepting XMVECTORF32 and define colors in TEN::Renderer::Colors namespace later in this file
#define PRINTSTRING_COLOR_ORANGE ARGB_TO_UINT(255, 216, 117, 49)
#define PRINTSTRING_COLOR_WHITE ARGB_TO_UINT(255, 255, 255, 255)
#define PRINTSTRING_COLOR_BLACK ARGB_TO_UINT(255, 0, 0, 0)
#define PRINTSTRING_COLOR_YELLOW ARGB_TO_UINT(255, 240, 220, 32)

constexpr auto MAX_LINES_2D		= 256;
constexpr auto MAX_LINES_3D		= 16384;
constexpr auto MAX_TRIANGLES_3D = 16384;

constexpr auto FADE_FACTOR = 0.0625f;

constexpr auto NUM_LIGHTS_PER_BUFFER = 48;
constexpr auto MAX_LIGHTS_PER_ITEM = 8;
constexpr auto MAX_LIGHTS_PER_ROOM = 48;
constexpr auto MAX_LIGHTS = 100;
constexpr auto AMBIENT_LIGHT_INTERPOLATION_STEP = 1.0f / 10.0f;
constexpr auto MAX_DYNAMIC_SHADOWS = 1;
constexpr auto MAX_DYNAMIC_LIGHTS = 1024;
constexpr auto ITEM_LIGHT_COLLECTION_RADIUS = BLOCK(2);
constexpr auto CAMERA_LIGHT_COLLECTION_RADIUS = BLOCK(4);

constexpr auto MAX_TRANSPARENT_FACES		  = 16384;
constexpr auto MAX_TRANSPARENT_VERTICES		  = MAX_TRANSPARENT_FACES * 6;
constexpr auto MAX_TRANSPARENT_FACES_PER_ROOM = 16384;
constexpr auto TRANSPARENT_BUCKET_SIZE		  = 3840 * 16;
constexpr auto ALPHA_TEST_THRESHOLD			  = 0.5f;
constexpr auto ALPHA_BLEND_THRESHOLD		  = 1.0f - EPSILON;
constexpr auto FAST_ALPHA_BLEND_THRESHOLD	  = 0.5f;

constexpr auto MAX_BONES = 32;
constexpr auto MAX_BONE_WEIGHTS = 4;

constexpr auto DISPLAY_SPACE_RES = Vector2(800.0f, 600.0f);
constexpr auto REFERENCE_FONT_SIZE = 35.0f;
constexpr auto HUD_ZERO_Y = -DISPLAY_SPACE_RES.y;

constexpr float DISPLAY_ITEM_NEAR_PLANE = 0.1f;
constexpr float DISPLAY_ITEM_FAR_PLANE = BLOCK(100);

constexpr auto UNDERWATER_FOG_MIN_DISTANCE = 4;
constexpr auto UNDERWATER_FOG_MAX_DISTANCE = 30;
constexpr auto MAX_ROOM_BOUNDS = 256;

constexpr auto MIN_FAR_VIEW = 3200.0f;
constexpr auto DEFAULT_FAR_VIEW = 102400.0f;

constexpr auto INSTANCED_SPRITES_BUCKET_SIZE = 512;
constexpr auto MAX_SPRITE_VERTICES 			 = INSTANCED_SPRITES_BUCKET_SIZE * 6;

constexpr auto SKY_TILES_COUNT = 20;
constexpr auto SKY_SIZE = 10240.0f;
constexpr auto SKY_VERTICES_COUNT = 4 * SKY_TILES_COUNT * SKY_TILES_COUNT;
constexpr auto SKY_INDICES_COUNT = 6 * SKY_TILES_COUNT * SKY_TILES_COUNT;
constexpr auto SKY_TRIANGLES_COUNT = 2 * SKY_TILES_COUNT * SKY_TILES_COUNT;

constexpr auto MAX_ROOMS_DRAW = 256;
constexpr auto MAX_ITEMS_DRAW = 128;
constexpr auto MAX_LIGHTS_DRAW = 48;
constexpr auto MAX_FOG_BULBS_DRAW = 32;
constexpr auto MAX_SPRITES_DRAW = 512;
constexpr auto MAX_LENS_FLARES_DRAW = 8;

constexpr auto ROOM_AMBIENT_MAP_SIZE = 512;
constexpr auto LEGACY_REFLECTIONS_DOWNSCALE_FACTOR = 2.0f;
constexpr auto MAX_ROOM_AMBIENT_MAPS = 10;

constexpr auto GLOW_DOWNSCALE_FACTOR = 4.0f;
constexpr auto GLOW_BLUR_SIGMA = 10.0f;
constexpr auto GLOW_BLUR_RADIUS = 24.0f;
constexpr auto INVENTORY_GLOW_BLUR_SIGMA = 4.0f;
constexpr auto INVENTORY_GLOW_BLUR_RADIUS = 8.0f;

constexpr auto GLOW_VERTEX_SHIFT = 0;
constexpr auto MOVE_VERTEX_SHIFT = 8;
constexpr auto SHININESS_VERTEX_SHIFT = 16;
constexpr auto LOCKED_VERTEX_SHIFT = 24;
constexpr auto INDEX_IN_POLY_VERTEX_SHIFT = 25;

enum class LightType
{
	Sun = 0,
	Point = 1,
	Spot = 2,
	Shadow = 3,
	FogBulb = 4
};

enum class BlendMode
{
	Unknown = -1,
	Opaque = 0,
	AlphaTest = 1,
	Additive = 2,
	NoDepthTest = 4,
	Subtractive = 5,
	Wireframe = 6,
	Exclude = 8,
	Screen = 9,
	Lighten = 10,
	AlphaBlend = 11,
	FastAlphaBlend = 12,
	PremultipliedAlphaBlend = 13
};

enum class SkinningMode
{
	None = 0,
	Full = 1,
	Classic = 2
};

enum class CullMode
{
	Unknown = -1,
	None = 0,
	Clockwise = 1,
	CounterClockwise = 2,
	Wireframe = 3
};

enum class ShadowMode
{
	None,
	Player,
	All
};

enum class AntialiasingMode
{
	None,
	Low,
	Medium,
	High
};

enum class LightMode
{
	Dynamic,
	Static
};

enum class DepthState
{
	Unknown = -1,
	Write = 0,
	Read = 1,
	None = 2
};

enum class SpriteType
{
	Billboard,
	ThreeD,
	CustomBillboard,
	LookAtBillboard
};

enum class RendererDebugPage
{
	None,
	RendererStats,
	MemoryStats,
	DimensionStats,
	PlayerStats,
	InputStats,
	CollisionStats,
	CollisionMeshStats,
	PortalStats,
	PathfindingStats,
	WireframeMode,

	Count
};

enum class TransparentFaceType
{
	Room,
	Moveable,
	Static,
	Sprite,
	None
};

enum class TextureRegister
{
	ColorMap = 0,
	NormalMap = 1,
	CausticsMap = 2,
	ShadowMap = 3,
	GBufferNormalMap = 4,
	Hud = 5,
	GBufferDepthMap = 6,
	EnvironmentMapFront = 7,
	EnvironmentMapBack = 8,
	SSAO = 9,
	ORSHMap = 10,
	EmissiveMap = 11,
	LegacyEnvironmentReflections = 12,
	SkyboxEnvironmentReflections = 13
};

enum class SamplerStateRegister
{
	None = 0,
	PointWrap = 1,
	LinearWrap = 2,
	LinearClamp = 3,
	AnisotropicWrap = 4,
	AnisotropicClamp = 5,
	ShadowMap = 6
};

enum class ConstantBufferRegister
{
	Camera = 0,
	Item = 1,
	Material = 2,
	InstancedStatics = 3,
	ShadowLight = 4,
	Room = 5,
	AnimatedTextures = 6,
	PostProcess = 7,
	Sky = 8,
	Hud = 10,
	HudBar = 11,
	Blending = 12,
	InstancedSprites = 13
};

enum class AlphaTestMode
{
	None = 0,
	GreatherThan = 1,
	LessThan = 2
};

enum class PrintStringFlags
{
	Center			= (1 << 0),
	Blink			= (1 << 1),
	Right			= (1 << 2),
	Outline			= (1 << 3),
	VerticalCenter	= (1 << 4),
	VerticalBottom	= (1 << 5)
};

enum class RendererPass
{
	ShadowMap,
	Opaque,
	Transparent,
	CollectTransparentFaces,
	Additive,
	GBuffer,
	GunFlashes,
	RoomAmbient
};

enum class SceneRenderMode
{
	Full,
	NoHud,
	NoPostprocess
};

enum class SpriteRenderType
{
	Default,
	LaserBarrier,
	LaserBeam
};

enum class RendererObjectType
{
	Unknown,
	Room,
	RoomPolygon,
	Moveable, 
	Static,
	Sprite,
	MoveableAsStatic, // For rats, bats, spiders, beetles
	HairPrimary,
	HairSecondary
};

enum class TextureSource
{
	Rooms,
	Moveables,
	Statics,
	Animated
};

enum class SMAAMode
{
	MODE_SMAA_1X,
	MODE_SMAA_T2X,
	MODE_SMAA_S2X,
	MODE_SMAA_4X,

	MODE_SMAA_COUNT = MODE_SMAA_4X
};

enum class SMAAPreset
{
	SMAA_PRESET_LOW,
	SMAA_PRESET_MEDIUM,
	SMAA_PRESET_HIGH,
	SMAA_PRESET_ULTRA,
	SMAA_PRESET_CUSTOM,

	SMAA_PRESET_COUNT = SMAA_PRESET_CUSTOM
};

enum class SMAAInput
{
	SMAA_INPUT_LUMA,
	SMAA_INPUT_COLOR,
	SMAA_INPUT_DEPTH,

	SMAA_INPUT_COUNT = SMAA_INPUT_DEPTH
};

enum class PostProcessMode
{
	None = 0,
	Monochrome = 1,
	Negative = 2,
	Exclusion = 3
};

enum class MaterialShaderType
{
	Default = 0,
	Reflective = 1,
	SkyboxReflective = 2
};

enum class SurfaceFormat
{
	Unknown,
	SF_RGBA8_Unorm,
	SF_RGBA8_Unorm_Srgb,
	SF_RG8_Unorm,
	SF_R8_Unorm,
	SF_R32_Float,
	SF_RGBA32_Float,
	SF_BGRA8_Unorm
};

enum class DepthFormat
{
	None,
	Depth24Stencil8,
	Depth32
};

enum class VertexInputFormat
{
	VI_RGBA8_Unorm,
	VI_RGBA8_Snorm,
	VI_RGBA8_Uint,
	VI_RG8_Unorm,
	VI_R32_Float,
	VI_R32_Uint,
	VI_RG32_Float,
	VI_RGB32_Float,
	VI_RGBA32_Float
};

enum class InputLayout
{
	Vertex,
	PostProcessVertex
};

enum class PrimitiveType
{
	TriangleList,
	TriangleStrip,
	LineList
};

enum class DepthStencilClearFlags
{
	Depth,
	Stencil,
	DepthAndStencil
};

enum class SpriteSortingMode
{
	BackToFront,
	FrontToBack,
	Deferred,
	Immediate,
	Texture
};

enum class VertexType
{
	Vertex,
	PostProcessVertex
};

enum class ShaderType
{
	Pixel,
	Vertex,
	PixelAndVertex,
	Compute,
	Geometry
};

enum class ShaderStage
{
	VertexShader,
	GeometryShader,
	PixelShader,
	ComputeShader,
	HullShader,
	DomainShader
};

enum class Shader
{
	// General

	None,
	Rooms,
	RoomsTransparent,
	RoomAmbient,
	RoomAmbientSky,
	Items,
	InstancedStatics,
	InstancedSprites,
	Sky,
	Solid,
	Inventory,
	FullScreenQuad,
	ShadowMap,

	// HUD

	Hud,
	HudColor,
	HudDTexture,
	HudBarColor,

	// GBuffer

	GBuffer,
	GBufferRooms,
	GBufferItems,
	GBufferInstancedStatics,

	// SMAA

	SmaaEdgeDetection,
	SmaaLumaEdgeDetection,
	SmaaColorEdgeDetection,
	SmaaDepthEdgeDetection,
	SmaaBlendingWeightCalculation,
	SmaaNeighborhoodBlending,
	Fxaa,

	// Post-process

	PostProcess,
	PostProcessMonochrome,
	PostProcessNegative,
	PostProcessExclusion,
	PostProcessFinalPass,
	PostProcessLensFlare,

	// SSAO

	Ssao,
	SsaoBlur,

	// Fullscreen effects

	Blur,
	Downscale,
	GlowCombine,

	Count
};

namespace TEN::Renderer::Colors
{
	XMGLOBALCONST XMVECTORF32 Transparent = { { { 0.f, 0.f, 0.f, 0.f } } };
	XMGLOBALCONST XMVECTORF32 Black = { { { 0.f, 0.f, 0.f, 1.f } } };
	XMGLOBALCONST XMVECTORF32 White = { { { 1.f, 1.f, 1.f, 1.f } } };
	XMGLOBALCONST XMVECTORF32 DimGray = { { { 0.412f, 0.412f, 0.412f, 1.f } } };
}