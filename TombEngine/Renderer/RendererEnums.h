#pragma once

#include "Math/Math.h"

#define SHAPE_RECTANGLE 0

#define PRINTSTRING_COLOR_ORANGE ARGB_TO_UINT(255, 216, 117, 49)
#define PRINTSTRING_COLOR_WHITE ARGB_TO_UINT(255, 255, 255, 255)

constexpr auto MAX_LINES_3D		= 16384;
constexpr auto MAX_TRIANGLES_3D = 16384;

constexpr auto FADE_FACTOR = 0.0625f;

constexpr auto NUM_LIGHTS_PER_BUFFER = 48;
constexpr auto MAX_LIGHTS_PER_ITEM = 8;
constexpr auto AMBIENT_LIGHT_INTERPOLATION_STEP = 1.0f / 10.0f;
constexpr auto MAX_DYNAMIC_LIGHTS = 1024;
constexpr auto ITEM_LIGHT_COLLECTION_RADIUS = BLOCK(2);
constexpr auto CAMERA_LIGHT_COLLECTION_RADIUS = BLOCK(4);

constexpr auto ALPHA_TEST_THRESHOLD  = 0.5f;
constexpr auto ALPHA_BLEND_THRESHOLD = 1.0f - EPSILON;

constexpr auto BONE_COUNT_MAX = 32;

constexpr auto DISPLAY_SPACE_RES = Vector2(800.0f, 600.0f);
constexpr auto DISPLAY_SPACE_ASPECT = DISPLAY_SPACE_RES.x / DISPLAY_SPACE_RES.y;
constexpr auto REFERENCE_FONT_SIZE = 35.0f;

// Near/far planes for the display item virtual camera. These are intentionally tight to maximise
// depth buffer precision and prevent z-fighting. Display items are typically rendered ~BLOCK(1)
// from the camera; the far plane of BLOCK(5) is generous but avoids the catastrophic near/far
// ratio (previously 1 000 000 : 1) that caused z-fighting between mesh faces.
constexpr float DISPLAY_ITEM_NEAR_PLANE = 0.01f;
constexpr float DISPLAY_ITEM_FAR_PLANE  = BLOCK(10);

constexpr auto MIN_FAR_VIEW = 3200.0f;
constexpr auto DEFAULT_FAR_VIEW = 102400.0f;

constexpr auto SKY_TILES_COUNT = 20;
constexpr auto SKY_SIZE = 10240.0f;
constexpr auto SKY_VERTICES_COUNT = 4 * SKY_TILES_COUNT * SKY_TILES_COUNT;
constexpr auto SKY_INDICES_COUNT = 6 * SKY_TILES_COUNT * SKY_TILES_COUNT;

constexpr auto MAX_ITEMS_DRAW = 128;
constexpr auto MAX_LIGHTS_DRAW = 48;
constexpr auto MAX_FOG_BULBS_DRAW = 32;
constexpr auto MAX_LENS_FLARES_DRAW = 8;

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
	Distortion = 3,
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
	// Values are uploaded directly as the skinning mode of the object uniforms; the vertex shader picks the
	// world transform path from it. Static is for instanced static meshes (no bones at
	// all); the others are for moveables where every vertex carries a BoneIndex.
	Static  = 0, // No bones - use the per-instance Object.World directly.
	None    = 1, // Rigid moveable - apply Bones[BoneIndex[0]] without blending.
	Full    = 2, // Modern skinning - blend bone matrices.
	Classic = 3, // Classic skinning - blend rotations as quaternions, keep the first bone translation.
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

// Resolution of the 3D scene relative to the screen; the scene is upscaled with AMD FSR 1. The values are stored in the configuration.
enum class RenderScaleMode
{
	Native,
	UltraQuality,
	Quality,
	Balanced,
	Performance,

	Count
};

enum class LightMode
{
	Dynamic,
	Static
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
	DimensionStats,
	PlayerStats,
	InputStats,
	CollisionStats,
	CollisionMeshStats,
	PortalStats,
	PathfindingStats,
	WireframeMode,
	SkyDebug,

	Count
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

enum class SceneRenderMode
{
	Full,
	NoHud
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
	Effect, // For items carrying FX data (body parts, projectiles)
	HairPrimary,
	HairSecondary
};

enum class PostProcessMode
{
	None = 0,
	Monochrome = 1,
	Negative = 2,
	Exclusion = 3
};

enum class DOFMode
{
	None = 0,
	Full = 1,
	Front = 2,
	Back = 3
};

enum class TextureMaterialType
{
	Default = 0,
	Reflective = 1,
	SkyboxReflective = 2
};

