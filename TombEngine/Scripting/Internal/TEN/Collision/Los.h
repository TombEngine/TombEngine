#pragma once

#include "Game/collision/Los.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"
#include "Scripting/Internal/TEN/Objects/Static/StaticObject.h"

using namespace TEN::Collision::Los;

namespace sol { class state; };

namespace TEN::Scripting::Collision
{
    class Ray
    {
    public:
        static void Register(sol::table& parent);

    private:
        // Fields
		LosCollisionData _RayCollisionData;
		Vector3 _origin;
		Vector3 _direction;
		float _distance;

    public:
        // Constructors

		Ray() = default;
		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist);
		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, bool collideMoveables);
		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, bool collideMoveables, bool collideStatics);
		Ray(const Vec3& origin, int roomNumber, const Rotation& rot, float dist);
		Ray(const Vec3& origin, int roomNumber, const Rotation& rot, float dist, bool collideMoveables);
		Ray(const Vec3& origin, int roomNumber, const Rotation& rot, float dist, bool collideMoveables, bool collideStatics);
		Ray(const Vec3& origin, int roomNumber, const Rotation& rot, const Vec3& relOffset);
		Ray(const Vec3& origin, int roomNumber, const Rotation& rot, const Vec3& relOffset, bool collideMoveables);
		Ray(const Vec3& origin, int roomNumber, const Rotation& rot, const Vec3& relOffset, bool collideMoveables, bool collideStatics);
        // Getters

		sol::optional <std::unique_ptr<Room>> GetRoom();
		sol::optional<Vec3> GetRoomPosition();
		sol::optional<std::string> GetRoomName();
		sol::optional<int> GetRoomNumber();
		sol::optional<float> GetRoomDistance();
		sol::optional<std::unique_ptr<Moveable>> GetMoveable();
		sol::optional<Vec3> GetMoveablePosition();
		sol::optional<float> GetMoveableDistance();
		sol::optional<std::unique_ptr<Static>> GetStatic();
		sol::optional<Vec3> GetStaticPosition();
		sol::optional<float> GetStaticDistance();

		// Inquirers

		bool HitMoveable(TypeOrNil<std::string> moveableName);
		bool HitStatic(TypeOrNil<std::string> staticName);
		bool HitRoom(TypeOrNil<std::string> roomName);

		// Utilities

		void Preview();
    };
}
