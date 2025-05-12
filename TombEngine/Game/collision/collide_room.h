#pragma once
#include "Game/collision/floordata.h"
#include "Math/Math.h"
#include "Objects/game_object_ids.h"

enum RoomEnvFlags;
class FloorInfo;
struct ItemInfo;
struct MESH_INFO;
struct RoomData;

using namespace TEN::Collision::Floordata;
using namespace TEN::Math;

constexpr auto NO_LOWER_BOUND = -NO_HEIGHT;	// Used by coll->Setup.LowerFloorBound.
constexpr auto NO_UPPER_BOUND = NO_HEIGHT;	// Used by coll->Setup.UpperFloorBound.
constexpr auto COLLISION_CHECK_DISTANCE = BLOCK(8);

constexpr auto DEFAULT_STEEP_FLOOR_SLOPE_ANGLE   = ANGLE(36.0f);
constexpr auto DEFAULT_STEEP_CEILING_SLOPE_ANGLE = ANGLE(45.0f);

enum class CollisionType
{
	None,
	Front,
	Left,
	Right,
	Top,
	TopFront,
	Clamp
};

enum class CollisionProbeMode
{
	Quadrants,
	FreeForward,
	FreeFlat
};

enum class CornerType
{
	None,
	Inner,
	Outer
};

struct CollisionPositionData
{
	int	  Floor		   = 0;
	int	  Ceiling	   = 0;
	int	  Bridge	   = 0;
	short SplitAngle   = 0;
	bool  FloorSlope   = false;
	bool  CeilingSlope = false;
	bool  DiagonalStep = false;

	bool HasDiagonalSplit()		   { return ((SplitAngle == SectorSurfaceData::SPLIT_ANGLE_0) || (SplitAngle == SectorSurfaceData::SPLIT_ANGLE_1)); }
	bool HasFlippedDiagonalSplit() { return (HasDiagonalSplit() && (SplitAngle != SectorSurfaceData::SPLIT_ANGLE_0)); }
};

struct CollisionSetupData
{
	// Collider parameters
	CollisionProbeMode Mode = CollisionProbeMode::Quadrants;
	int   Radius	   = 0;
	int   Height	   = 0;
	short ForwardAngle = 0;

	// Borderline step heights
	int LowerFloorBound	  = 0;
	int UpperFloorBound	  = 0;
	int LowerCeilingBound = 0;
	int UpperCeilingBound = 0;

	// Blocker flags
	bool BlockFloorSlopeUp	  = false;
	bool BlockFloorSlopeDown  = false;
	bool BlockCeilingSlope	  = false;
	bool BlockDeathFloorDown  = false;
	bool BlockMonkeySwingEdge = false;
	
	// Inquirers
	bool EnableObjectPush	= false;
	bool EnableSpasm		= false;
	bool ForceSolidStatics	= false;

	// Previous parameters
	Vector3i	   PrevPosition		= Vector3i::Zero;
	GAME_OBJECT_ID PrevAnimObjectID = ID_NO_OBJECT;
	int			   PrevAnimNumber	= 0;
	int			   PrevFrameNumber	= 0;
	int			   PrevState		= 0;
};

struct CollisionInfo
{
	CollisionSetupData Setup = {};

	CollisionPositionData Middle	  = {};
	CollisionPositionData MiddleLeft  = {};
	CollisionPositionData MiddleRight = {};
	CollisionPositionData Front		  = {};
	CollisionPositionData FrontLeft	  = {};
	CollisionPositionData FrontRight  = {};

	CollisionType CollisionType = CollisionType::None;
	Pose		  Shift			= Pose::Zero;

	Vector3 FloorNormal			 = Vector3::Zero;
	Vector3 CeilingNormal		 = Vector3::Zero;
	Vector2 FloorTilt			 = Vector2::Zero; // NOTE: Deprecated.
	Vector2 CeilingTilt			 = Vector2::Zero; // NOTE: Deprecated.
	short	NearestLedgeAngle	 = 0;
	float	NearestLedgeDistance = 0.0f;

	int  LastBridgeItemNumber = 0;
	Pose LastBridgeItemPose	  = Pose::Zero;

	bool HitStatic	   = false;
	bool HitTallObject = false;

	// Inquirers.
	bool TriangleAtRight()	   { return ((MiddleRight.SplitAngle != 0) && (MiddleRight.SplitAngle == Middle.SplitAngle)); }
	bool TriangleAtLeft()	   { return ((MiddleLeft.SplitAngle != 0) && (MiddleLeft.SplitAngle == Middle.SplitAngle)); }
	bool DiagonalStepAtRight() { return (MiddleRight.DiagonalStep && TriangleAtRight() && (NearestLedgeAngle % ANGLE(90.0f))); }
	bool DiagonalStepAtLeft()  { return (MiddleLeft.DiagonalStep && TriangleAtLeft() && (NearestLedgeAngle % ANGLE(90.0f))); }
};

[[nodiscard]] bool TestItemRoomCollisionAABB(ItemInfo* item);

void  GetCollisionInfo(CollisionInfo* coll, ItemInfo* item, bool resetRoom = false);
void  GetCollisionInfo(CollisionInfo* coll, ItemInfo* item, const Vector3i& offset, bool resetRoom = false);
int	  GetQuadrant(short angle);
short GetNearestLedgeAngle(ItemInfo* item, CollisionInfo* coll, float& distance);

FloorInfo* GetFloor(int x, int y, int z, short* roomNumber);
int GetFloorHeight(FloorInfo* floor, int x, int y, int z);
int GetCeiling(FloorInfo* floor, int x, int y, int z);
int GetDistanceToFloor(int itemNumber, bool precise = true);

int  FindGridShift(int x, int z);
void ShiftItem(ItemInfo* item, CollisionInfo* coll);
void SnapItemToLedge(ItemInfo* item, CollisionInfo* coll, float offsetMultiplier = 0.0f, bool snapToAngle = true);
void SnapItemToLedge(ItemInfo* item, CollisionInfo* coll, short angle, float offsetMultiplier = 0.0f);
void SnapItemToGrid(ItemInfo* item, CollisionInfo* coll);

void AlignEntityToSurface(ItemInfo* item, const Vector2& ellipse, float alpha = 0.75f, short constraintAngle = ANGLE(70.0f));

// TODO: Deprecated.
bool TestEnvironment(RoomEnvFlags environmentType, int x, int y, int z, int roomNumber);
bool TestEnvironment(RoomEnvFlags environmentType, const Vector3i& pos, int roomNumber);
bool TestEnvironment(RoomEnvFlags environmentType, const ItemInfo* item);
bool TestEnvironment(RoomEnvFlags environmentType, int roomNumber);
bool TestEnvironment(RoomEnvFlags environmentType, const RoomData* room);
bool TestEnvironmentFlags(RoomEnvFlags environmentType, int flags);
