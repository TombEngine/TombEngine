#pragma once

#include "Math/Math.h"

// Constants and enums shared by the renderer frontend, the scene renderer backend and the game code.

// Polygon shape value of a quad (see RendererPolygon::Shape); any other value is a triangle.
#define SHAPE_RECTANGLE 0

#define PRINTSTRING_COLOR_ORANGE ARGB_TO_UINT(255, 216, 117, 49)
#define PRINTSTRING_COLOR_WHITE ARGB_TO_UINT(255, 255, 255, 255)

// Capacity of the preallocated debug primitive lists.
constexpr auto MAX_LINES_3D		= 16384;
constexpr auto MAX_TRIANGLES_3D = 16384;

constexpr auto FADE_FACTOR = 0.0625f;

// Light limits: dynamic lights in the view light list, and lights per item.
constexpr auto NUM_LIGHTS_PER_BUFFER = 48;
constexpr auto MAX_LIGHTS_PER_ITEM = 8;
constexpr auto AMBIENT_LIGHT_INTERPOLATION_STEP = 1.0f / 10.0f;
constexpr auto MAX_DYNAMIC_LIGHTS = 1024;
constexpr auto ITEM_LIGHT_COLLECTION_RADIUS = BLOCK(2);
constexpr auto CAMERA_LIGHT_COLLECTION_RADIUS = BLOCK(4);

// Opacity limits: below ALPHA_TEST_THRESHOLD the alpha test discards pixels; items at or above ALPHA_BLEND_THRESHOLD
// are treated as fully opaque and may be batched, translucent ones switch to alpha blending.
constexpr auto ALPHA_TEST_THRESHOLD  = 0.5f;
constexpr auto ALPHA_BLEND_THRESHOLD = 1.0f - EPSILON;

// Maximum bones of a moveable; sizes the per-item transform arrays.
constexpr auto BONE_COUNT_MAX = 32;

constexpr auto DISPLAY_SPACE_RES = Vector2(800.0f, 600.0f);
constexpr auto DISPLAY_SPACE_ASPECT = DISPLAY_SPACE_RES.x / DISPLAY_SPACE_RES.y;
constexpr auto REFERENCE_FONT_SIZE = 35.0f;

// Near/far planes for the display item virtual camera. These are intentionally tight to maximise
// depth buffer precision and prevent z-fighting. Display items are typically rendered ~BLOCK(1)
// from the camera; the far plane of BLOCK(10) is generous but avoids the catastrophic near/far
// ratio (previously 1 000 000 : 1) that caused z-fighting between mesh faces.
constexpr float DISPLAY_ITEM_NEAR_PLANE = 0.01f;
constexpr float DISPLAY_ITEM_FAR_PLANE  = BLOCK(10);

// Far plane limits of the game camera: a smaller requested far view falls back to the default.
constexpr auto MIN_FAR_VIEW = 3200.0f;
constexpr auto DEFAULT_FAR_VIEW = 102400.0f;

// Classic sky layer plane: a grid of SKY_TILES_COUNT x SKY_TILES_COUNT tiles, SKY_SIZE wide.
constexpr auto SKY_TILES_COUNT = 20;
constexpr auto SKY_SIZE = 10240.0f;
constexpr auto SKY_VERTICES_COUNT = 4 * SKY_TILES_COUNT * SKY_TILES_COUNT;
constexpr auto SKY_INDICES_COUNT = 6 * SKY_TILES_COUNT * SKY_TILES_COUNT;

// Capacity limits of per-frame lists.
constexpr auto MAX_ITEMS_DRAW = 128;
constexpr auto MAX_LIGHTS_DRAW = 48;
constexpr auto MAX_FOG_BULBS_DRAW = 32;
constexpr auto MAX_LENS_FLARES_DRAW = 8;

// Bit offsets of the packed Vertex::Effects field (see Renderer::PackEffectsAndIndexInPoly).
constexpr auto GLOW_VERTEX_SHIFT = 0;
constexpr auto MOVE_VERTEX_SHIFT = 8;
constexpr auto SHININESS_VERTEX_SHIFT = 16;
constexpr auto LOCKED_VERTEX_SHIFT = 24;
constexpr auto INDEX_IN_POLY_VERTEX_SHIFT = 25;

// Light kinds of the room and dynamic light lists; FogBulb lights are not lights but volumes collected as fog bulbs.
enum class LightType
{
	Sun = 0,
	Point = 1,
	Spot = 2,
	Shadow = 3,
	FogBulb = 4
};

// Blend modes of faces and sprites. Opaque, AlphaTest, Additive and Distortion are drawn in batches; the
// other modes are depth sorted with the transparent faces.
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

// Which items cast shadows: none, only the player, or all items.
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

// Dynamic meshes are lit by the lights collected for them; static ones keep only their baked vertex colors.
enum class LightMode
{
	Dynamic,
	Static
};

// Billboard faces the camera; CustomBillboard rotates around a constrained axis; LookAtBillboard is oriented along an axis;
// ThreeD is a quad with explicit world space corners.
enum class SpriteType
{
	Billboard,
	ThreeD,
	CustomBillboard,
	LookAtBillboard
};

// Pages of the debug overlay; Count is the number of pages, not a page.
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

// Bit flags of a printed string; combine with bitwise or.
enum class PrintStringFlags
{
	Center			= (1 << 0),
	Blink			= (1 << 1),
	Right			= (1 << 2),
	Outline			= (1 << 3),
	VerticalCenter	= (1 << 4),
	VerticalBottom	= (1 << 5)
};

// Full draws the HUD with the scene; NoHud captures the scene alone, e.g. for the menu background.
enum class SceneRenderMode
{
	Full,
	NoHud
};

// Special rendering paths of sprites, such as laser effects; Default is the regular one.
enum class SpriteRenderType
{
	Default,
	LaserBarrier,
	LaserBeam
};

// Kind of object a sorted face or sprite belongs to.
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

// Full screen color effects set by the game code.
enum class PostProcessMode
{
	None = 0,
	Monochrome = 1,
	Negative = 2,
	Exclusion = 3
};

// Depth of field: Full blurs foreground and background, Front only the foreground, Back only the background.
enum class DOFMode
{
	None = 0,
	Full = 1,
	Front = 2,
	Back = 3
};

// Material class of a texture; reflective materials sample the environment, skybox reflective ones the sky.
enum class TextureMaterialType
{
	Default = 0,
	Reflective = 1,
	SkyboxReflective = 2
};

