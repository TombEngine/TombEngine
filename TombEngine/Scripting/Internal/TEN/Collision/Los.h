#pragma once

#include "Game/collision/Los.h"
#include "Scripting/Internal/TEN/Collision/IntersectionTypes.h"
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
		bool             _penetrate = false;

		// Utilities

		bool IsOccluded(float dist) const;

	public:
		// Constructors

		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist);
		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, ScriptIntersectionType hitMoveables);
		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, ScriptIntersectionType hitMoveables, ScriptIntersectionType hitStatics);
		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, ScriptIntersectionType hitMoveables, ScriptIntersectionType hitStatics, bool penetrate);

		// Getters

		sol::optional<std::unique_ptr<Room>>     GetRoom();
		sol::optional<Vec3>                      GetRoomPosition();
		sol::optional<Vec3>                      GetRoomNormal();
		sol::optional<float>                     GetRoomDistance();
		sol::optional<std::unique_ptr<Moveable>> GetMoveable();
		sol::optional<Vec3>                      GetMoveablePosition();
		sol::optional<float>                     GetMoveableDistance();
		sol::optional<std::unique_ptr<Static>>   GetStatic();
		sol::optional<Vec3>                      GetStaticPosition();
		sol::optional<float>                     GetStaticDistance();

		sol::optional<std::vector <std::unique_ptr<Moveable>>> GetMoveables();
		sol::optional<std::vector<std::unique_ptr<Static>>> GetStatics();

		// Inquirers

		bool HitRoom(const TypeOrNil<std::string>& name);
		bool HitMoveable(const TypeOrNil<std::string>& name);
		bool HitStatic(const TypeOrNil<std::string>& name);

		// Utilities

		void Preview();
	};
}
