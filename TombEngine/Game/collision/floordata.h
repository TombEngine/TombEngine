#pragma once
#include "Math/Math.h"
#include "Specific/newtypes.h"

using namespace TEN::Math;

struct ItemInfo;

// GLOSSARY OF TERMS
// Ceiling:			Upper surface of a sector.
// Floor:			Lower surface of a sector.
// Floordata:		Name of the engine's level geometry collision system composed of rooms with sectors.
// Location:		Vertical location within a room. TODO: Refine this concept and use a better name.
// Plane:			Mathematical representation of one of two surface triangles.
// Portal:			Link from one room to another allowing traversal between them.
// Room number:		Unique ID of a room.
// Room grid coord: Relative 2D grid coordinate of a room (e.g. [0, 0] denotes the first sector).
// Sector:			Collision data describing a single grid division within a room.
// Sector point:	Relative 2D position within a sector (range [0, BLOCK(1)) on each axis).
// Surface:			Floor or ceiling consisting of two triangles.
// Triangle:		Surface subdivision. Only 2 per surface can exist.
// Wall:			Inferred from a high floor or ceiling. Note that true "walls" don't exist in floordata, only surface heights.

enum class MaterialType
{
	Mud = 0,
	Snow = 1,
	Sand = 2,
	Gravel = 3,
	Ice = 4,
	Water = 5,
	Stone = 6,
	Wood = 7,
	Metal = 8,
	Marble = 9,
	Grass = 10,
	Concrete = 11,
	OldWood = 12,
	OldMetal = 13,
	Custom1 = 14,
	Custom2 = 15,
	Custom3 = 16,
	Custom4 = 17,
	Custom5 = 18,
	Custom6 = 19,
	Custom7 = 20,
	Custom8 = 21
};

enum class ClimbDirectionFlags
{
	North = 1 << 8,
	East  = 1 << 9,
	South = 1 << 10,
	West  = 1 << 11
};

// NOTE: Describes vertical room location.
class RoomVector 
{
public:
	// Fields

	int RoomNumber = 0;
	int Height	   = 0;

	// Constructors

	RoomVector() = default;
	RoomVector(int roomNumber, int height)
	{
		RoomNumber = roomNumber;
		Height = height;
	}
};

struct SectorSurfaceTriangleData
{
	Plane		 Plane			  = {};
	int			 PortalRoomNumber = 0;
	short		 SteepSlopeAngle  = 0;
	MaterialType Material		  = MaterialType::Stone;
};

struct SectorSurfaceData
{
private:
	static constexpr auto TRIANGLE_COUNT = 2;

public:
	static constexpr auto SPLIT_ANGLE_0 = ANGLE(45.0f);
	static constexpr auto SPLIT_ANGLE_1 = ANGLE(135.0f);

	short SplitAngle = 0;
	std::array<SectorSurfaceTriangleData, TRIANGLE_COUNT> Triangles = {};
};

struct SectorFlagData
{
	bool Death		 = false;
	bool Monkeyswing = false;
	bool ClimbNorth	 = false;
	bool ClimbSouth	 = false;
	bool ClimbWest	 = false;
	bool ClimbEast	 = false;
	bool MarkBeetle	 = false;

	bool MarkTriggerer		 = false;
	bool MarkTriggererActive = false; // TODO: Must be written to and read from savegames.

	bool MinecartLeft() { return MarkTriggerer; }
	bool MinecartRight() { return MarkBeetle; }
	bool MinecartStop() { return (MarkBeetle && MarkTriggerer); }

	bool IsWallClimbable(ClimbDirectionFlags flag) const
	{
		switch (flag)
		{
		case ClimbDirectionFlags::North:
			return ClimbNorth;

		case ClimbDirectionFlags::South:
			return ClimbSouth;

		case ClimbDirectionFlags::East:
			return ClimbEast;

		case ClimbDirectionFlags::West:
			return ClimbWest;
		}

		return false;
	}
};

// SectorData
class FloorInfo
{
public:
	// Fields

	int ID = 0;

	Vector2i		  Position		 = Vector2i::Zero;
	int				  RoomNumber	 = 0;
	BoundingBox		  Aabb			 = BoundingBox();
	SectorSurfaceData FloorSurface	 = {};
	SectorSurfaceData CeilingSurface = {};
	SectorFlagData	  Flags			 = {};

	std::set<int> BridgeItemNumbers	   = {};
	int			  SidePortalRoomNumber = 0;

	int	 PathfindingBoxID = 0;
	int	 TriggerIndex	  = 0;
	bool Stopper		  = true;

	// Getters

	int								 GetSurfaceTriangleID(int x, int z, bool isFloor) const;
	const SectorSurfaceTriangleData& GetSurfaceTriangle(int x, int z, bool isFloor) const;
	Vector3							 GetSurfaceNormal(int triID, bool isFloor) const;
	Vector3							 GetSurfaceNormal(int x, int z, bool isFloor) const;
	short							 GetSurfaceSteepSlopeAngle(int x, int z, bool isFloor) const;
	MaterialType					 GetSurfaceMaterial(int x, int z, bool isFloor) const;

	std::optional<int> GetNextRoomNumber(int x, int z, bool isBelow) const;
	std::optional<int> GetNextRoomNumber(const Vector3i& pos, bool isBelow) const;
	std::optional<int> GetSideRoomNumber() const;

	int GetSurfaceHeight(int x, int z, bool isFloor) const;
	int GetSurfaceHeight(const Vector3i& pos, bool isFloor) const;
	int GetBridgeSurfaceHeight(const Vector3i& pos, bool isFloor) const;

	// Inquirers

	bool IsSurfaceSplit(bool isFloor) const;
	bool IsSurfaceDiagonalStep(bool isFloor) const;
	bool IsSurfaceSplitPortal(bool isFloor) const;
	bool IsWall(int triID) const;
	bool IsWall(int x, int z) const;

	// Bridge utilities

	int	 GetInsideBridgeItemNumber(const Vector3i& pos, bool floorBorder, bool ceilingBorder) const;
	void AddBridge(int itemNumber);
	void RemoveBridge(int itemNumber);
};

namespace TEN::Collision::Floordata
{
	// Deprecated

	Vector2i GetSurfaceTilt(const Vector3& normal, bool isFloor);

	Vector2i				GetSectorPoint(int x, int z);
	Vector2i				GetRoomGridCoord(int roomNumber, int x, int z, bool clampToBounds = true);
	std::vector<Vector2i>	GetNeighborRoomGridCoords(const Vector3i& pos, int roomNumber, unsigned int searchDepth);
	std::vector<FloorInfo*> GetNeighborSectors(const Vector3i& pos, int roomNumber, unsigned int searchDepth);

	FloorInfo& GetFloor(int roomNumber, const Vector2i& roomGridCoord);
	FloorInfo& GetFloor(int roomNumber, int x, int z);
	FloorInfo& GetFarthestSector(int roomNumber, int x, int z, bool isBottom);
	FloorInfo& GetSideSector(int roomNumber, int x, int z);

	std::optional<int> GetSurfaceHeight(const RoomVector& location, int x, int z, bool isFloor);
	RoomVector		   GetRoomVector(RoomVector location, const Vector3i& pos);

	std::optional<int> GetBridgeItemIntersect(const ItemInfo& item, const Vector3i& pos, bool useBottomHeight);
	int				   GetBridgeBorder(const ItemInfo& item, bool isBottom);

	bool TestMaterial(MaterialType refMaterial, const std::vector<MaterialType>& materials);
	
	void DrawNearbySectorFlags(const ItemInfo& item);
}
