#pragma once

#include "Game/collision/Point.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"

using namespace TEN::Collision::Point;

namespace sol { class state; };
namespace TEN::Scripting { class Rotation; }
class Moveable;
class Vec3;

namespace TEN::Scripting::Collision
{
	// TODO:
	// Integrate line of sight queries (LOS).
	// Integrate moveable and static queries.

    class Probe
    {
    public:
        static void Register(sol::table& parent);

    private:
        // Fields

		PointCollisionData _pointCollision = PointCollisionData();

    public:
        // Constructors

		Probe() = default;
		Probe(const Vec3& pos);
		Probe(const Vec3& pos, int roomNumber);
		Probe(const Vec3& origin, int roomNumber, const Vec3& dir, float dist);
		Probe(const Vec3& origin, int roomNumber, const Rotation& rot, float dist);
		Probe(const Vec3& origin, int roomNumber, const Rotation& rot, const Vec3& relOffset);
		
        // Getters

		Vec3				  GetPosition();
		std::unique_ptr<Room> GetRoom();
		std::string			  GetRoomName();
		int					  GetRoomNumber();

		sol::optional<int>			GetFloorHeight();
		sol::optional<int>			GetCeilingHeight();
		sol::optional<int>			GetWaterSurfaceHeight();
		sol::optional<Vec3>			GetFloorNormal();
		sol::optional<Vec3>			GetCeilingNormal();
		sol::optional<MaterialType> GetFloorMaterialType();
		sol::optional<MaterialType> GetCeilingMaterialType();

		// Inquirers

		sol::optional<bool> IsSteepFloor();
		sol::optional<bool> IsSteepCeiling();
		bool				IsWall();
		bool				IsInsideSolidGeometry();
		bool				IsClimbableWall(float headingAngle);
		bool				IsMonkeySwing();
		bool				IsDeath();

		// Utilities

		void Preview();
    };

	void Register(sol::state* lua, sol::table& parent);
}
