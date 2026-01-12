#pragma once

#include "Game/collision/Los.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"
#include "Scripting/Internal/TEN/Objects/Static/StaticObject.h"

using namespace TEN::Collision::Los;

class Vec3;
namespace TEN::Scripting { class Rotation; }
namespace sol { class state; }

namespace TEN::Scripting::Collision
{
	class Ray
	{
	public:
		static void Register(sol::table& parent);

	private:
		// Fields

		LosCollisionData _los       = {};
		Vector3          _origin    = Vector3::Zero;
		Vector3          _direction = Vector3::Zero;
		float            _distance  = 0.0f;

	public:
		// Constructors

		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist);
		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, bool hitMoveables);
		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, bool hitMoveables, bool hitStatics);
		Ray(const Vec3& origin, int roomNumber, const Rotation& rot, float dist);
		Ray(const Vec3& origin, int roomNumber, const Rotation& rot, float dist, bool hitMoveables);
		Ray(const Vec3& origin, int roomNumber, const Rotation& rot, float dist, bool hitMoveables, bool hitStatics);

		// Getters

		sol::optional <std::unique_ptr<Room>>    GetRoom();
		sol::optional<Vec3>                      GetRoomPosition();
		sol::optional<float>                     GetRoomDistance();
		sol::optional<std::unique_ptr<Moveable>> GetMoveable();
		sol::optional<Vec3>                      GetMoveablePosition();
		sol::optional<float>                     GetMoveableDistance();
		sol::optional<std::unique_ptr<Static>>   GetStatic();
		sol::optional<Vec3>                      GetStaticPosition();
		sol::optional<float>                     GetStaticDistance();

		// Inquirers

		bool HitRoom(const TypeOrNil<std::string>& name);
		bool HitMoveable(const TypeOrNil<std::string>& name);
		bool HitStatic(const TypeOrNil<std::string>& name);

		// Utilities

		void Preview();
	};
}
