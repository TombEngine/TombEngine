#pragma once
#include "Game/collision/collide_room.h"
#include "Math/Math.h"

enum RoomEnvFlags;
class FloorInfo;
struct ItemInfo;

using namespace TEN::Math;

namespace TEN::Collision::Point
{
	class PointCollisionData
	{
	private:
		// Members

		Vector3i _position	= Vector3i::Zero;
		int		 _roomNumber = 0;

		FloorInfo* _sector		 = nullptr;
		FloorInfo* _bottomSector = nullptr;
		FloorInfo* _topSector	 = nullptr;

		std::optional<int>	   _floorHeight				= std::nullopt;
		std::optional<int>	   _ceilingHeight			= std::nullopt;
		std::optional<Vector3> _floorNormal				= std::nullopt;
		std::optional<Vector3> _ceilingNormal			= std::nullopt;
		std::optional<int>	   _floorBridgeItemNumber	= std::nullopt;
		std::optional<int>	   _ceilingBridgeItemNumber = std::nullopt;

		std::optional<int> _waterSurfaceHeight = std::nullopt;
		std::optional<int> _waterBottomHeight  = std::nullopt;
		std::optional<int> _waterTopHeight	   = std::nullopt;

	public:
		// Constructors

		PointCollisionData() = default;
		PointCollisionData(const Vector3i& pos, int roomNumber);

		// Getters

		Vector3i GetPosition() const;
		int		 GetRoomNumber() const;

		FloorInfo& GetSector();
		FloorInfo& GetBottomSector();
		FloorInfo& GetTopSector();

		int		GetFloorHeight();
		int		GetCeilingHeight();
		Vector3 GetFloorNormal();
		Vector3 GetCeilingNormal();
		int		GetFloorBridgeItemNumber();
		int		GetCeilingBridgeItemNumber();

		int GetWaterSurfaceHeight();
		int GetWaterBottomHeight();
		int GetWaterTopHeight();

		// Inquirers

		bool IsWall();
		bool IsSteepFloor();
		bool IsSteepCeiling();
		bool IsDiagonalFloorStep();
		bool IsDiagonalCeilingStep();
		bool IsDiagonalFloorSplit();
		bool IsDiagonalCeilingSplit();
		bool IsFlippedDiagonalFloorSplit();
		bool IsFlippedDiagonalCeilingSplit();

		bool TestEnvironmentFlag(RoomEnvFlags envFlag);

	private:
		// Helpers

		Vector3 GetBridgeNormal(bool isFloor);
	};

	PointCollisionData GetPointCollision(const Vector3i& pos, int roomNumber);
	PointCollisionData GetPointCollision(const Vector3i& pos, int roomNumber, const Vector3& dir, float dist);
	PointCollisionData GetPointCollision(const Vector3i& pos, int roomNumber, const Vector3& offset);
	PointCollisionData GetPointCollision(const Vector3i& pos, int roomNumber, short headingAngle, float forward, float down = 0.0f, float right = 0.0f,
										 const Vector3& axis = Vector3::UnitY);
	
	PointCollisionData GetPointCollision(const ItemInfo& item);
	PointCollisionData GetPointCollision(const ItemInfo& item, const Vector3& dir, float dist);
	PointCollisionData GetPointCollision(const ItemInfo& item, short headingAngle, float forward, float down = 0.0f, float right = 0.0f,
										 const Vector3& axis = Vector3::UnitY);
}
