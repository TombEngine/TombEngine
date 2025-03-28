#pragma once

#include "Math/Math.h"

class GameVector;
struct ItemInfo;
struct MESH_INFO;

using namespace TEN::Math;

namespace TEN::Collision::Los
{
	struct RoomLosCollisionData
	{
		std::optional<CollisionTriangleData> Triangle	= std::nullopt;
		Vector3								 Position	= Vector3::Zero;
		int									 RoomNumber = 0;

		std::vector<int> RoomNumbers   = {};
		float			 Distance	   = 0.0f;
		bool			 IsIntersected = false;
	};

	struct ItemLosCollisionData
	{
		ItemInfo* Item		 = nullptr;
		Vector3	  Position	 = Vector3::Zero;
		int		  RoomNumber = 0;

		float Distance			= 0.0f;
		bool  IsOriginContained = false;
	};

	struct SphereLosCollisionData
	{
		ItemInfo* Item		 = nullptr;
		int		  SphereID	 = 0;
		Vector3	  Position	 = Vector3::Zero;
		int		  RoomNumber = 0;

		float Distance			= 0.0f;
		bool  IsOriginContained = false;
	};

	struct StaticLosCollisionData
	{
		MESH_INFO* Static	  = nullptr;
		Vector3	   Position	  = Vector3::Zero;
		int		   RoomNumber = 0;

		float Distance			= 0.0f;
		bool  IsOriginContained = false;
	};

	struct LosCollisionData
	{
		RoomLosCollisionData				Room	= {};
		std::vector<ItemLosCollisionData>	Items	= {};
		std::vector<SphereLosCollisionData> Spheres = {};
		std::vector<StaticLosCollisionData> Statics = {};
	};

	// Low-level LOS collision

	LosCollisionData GetLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist,
									 bool collideItems, bool collideSpheres, bool collideStatics);

	// High-level LOS collision

	RoomLosCollisionData				  GetRoomLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collideBridges = true);
	std::optional<ItemLosCollisionData>	  GetItemLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collidePlayer = false);
	std::optional<SphereLosCollisionData> GetSphereLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collidePlayer = false);
	std::optional<StaticLosCollisionData> GetStaticLosCollision(const Vector3& origin, int roomNumber, const Vector3& dir, float dist, bool collideOnlySolid = true);

	std::pair<GameVector, GameVector> GetRayFrom2DPosition(const Vector2& screenPos);
}
