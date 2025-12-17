#include "framework.h"
#include "Scripting/Internal/TEN/Collision/Los.h"
#include "Scripting/Internal/TEN/Collision/MaterialTypes.h"

#include "Game/collision/Los.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"
#include "Scripting/Internal/TEN/Objects/Static/StaticObject.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Specific/level.h"

using namespace TEN::Collision::Los;
using namespace TEN::Scripting::Types;

namespace TEN::Scripting::Collision
{
	///Casts a collision Ray in the game world.
	// Provides collision information from collided room, item or static mesh.
	//
	// @tenclass Collision.Ray
	// @pragma nostrip

	void Ray::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			Ray(const Vec3&, int, const Vec3&, float),
			Ray(const Vec3&, int, const Vec3&, float, bool),
			Ray(const Vec3&, int, const Vec3&, float, bool, bool),
			Ray(const Vec3&, int, const Rotation&, float),
			Ray(const Vec3&, int, const Rotation&, float, bool),
			Ray(const Vec3&, int, const Rotation&, float, bool, bool),
			Ray(const Vec3&, int, const Rotation&, const Vec3&),
			Ray(const Vec3&, int, const Rotation&, const Vec3&, bool),
			Ray(const Vec3&, int, const Rotation&, const Vec3&, bool)>;

		// Register type.
		parent.new_usertype<Ray>(
			ScriptReserved_Ray,
			ctors(), sol::call_constructor, ctors(),

			// Getters
			ScriptReserved_RayGetRoom, &Ray::GetRoom,
			ScriptReserved_RayGetRoomPosition, &Ray::GetRoomPosition,
			ScriptReserved_RayGetRoomName, &Ray::GetRoomName,
			ScriptReserved_RayGetRoomNumber, &Ray::GetRoomNumber,
			ScriptReserved_RayGetRoomDistance, &Ray::GetRoomDistance,
			ScriptReserved_RayGetMoveable, &Ray::GetMoveable,
			ScriptReserved_RayGetMoveablePosition, & Ray::GetMoveablePosition,
			ScriptReserved_RayGetMoveableDistance, &Ray::GetMoveableDistance,
			ScriptReserved_RayGetStatic, &Ray::GetStatic,
			ScriptReserved_RayGetStaticPosition, & Ray::GetStaticPosition,
			ScriptReserved_RayGetStaticDistance, & Ray::GetStaticDistance,

			// Inquirers
			ScriptReserved_RayHitMoveable, &Ray::HitMoveable,
			ScriptReserved_RayHitStatic, &Ray::HitStatic,
			ScriptReserved_RayHitRoom, &Ray::HitRoom,
			
			// Utilities
			ScriptReserved_ProbePreview, &Ray::Preview);
	}

	/// Create a Ray at a specified world position, direction and distance in a room.
	// @function Ray
	// @tparam Vec3 pos World position.
	// @tparam int roomNumber Room number.
	// @tparam Vec3 direction Normal which indicates light direction
	// @tparam float dist Maximum distance the ray can travel.
	// @tparam[opt=false] bool collideMoveables Specfies if Ray should collide with moveables. Enabling this option is computationally expensive. Use only when necessary.
	// @tparam[opt=false] bool collideStatics Specfies if Ray should collide with statics. Enabling this option is computationally expensive. Use only when necessary.
	// @treturn Ray A new Ray.

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist)
	{
		_origin = origin;
		_direction = dir;
		_distance = dist;
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, false, false, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist,
		bool collideMoveables)
	{
		_origin = origin;
		_direction = dir;
		_distance = dist;
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, collideMoveables, false, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist,
		bool collideMoveables, bool collideStatics)
	{
		_origin = origin;
		_direction = dir;
		_distance = dist;
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, collideMoveables, false, collideStatics);
	}

	/// Create a Ray at a specified world position, in a room in the direction of a given rotation for a specified distance.
	// @function Ray
	// @tparam Vec3 pos World position.
	// @tparam int roomNumber Room number.
	// @tparam Rotation rot Rotation defining the direction in which to cast.
	// @tparam float dist Maximum distance the ray can travel.
	// @tparam[opt=false] bool collideMoveables Specfies if Ray should collide with moveables. Enabling this option is computationally expensive. Use only when necessary.
	// @tparam[opt=false] bool collideStatics Specfies if Ray should collide with statics. Enabling this option is computationally expensive. Use only when necessary.
	// @treturn Ray A new Ray.
	Ray::Ray(const Vec3& origin, int roomNumber, const Rotation& rot, float dist)
	{
		auto dir = rot.ToEulerAngles().ToDirection();
		_origin = origin;
		_direction = dir;
		_distance = dist;
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, false, false, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Rotation& rot, float dist, bool collideMoveables)
	{
		auto dir = rot.ToEulerAngles().ToDirection();
		_origin = origin;
		_direction = dir;
		_distance = dist;
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, collideMoveables, false, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Rotation& rot, float dist, bool collideMoveables, bool collideStatics)
	{
		auto dir = rot.ToEulerAngles().ToDirection();
		_origin = origin;
		_direction = dir;
		_distance = dist;
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, collideMoveables, false, collideStatics);
	}
	
	/// Create a Ray at a specified world position, where a given relative offset is rotated according to a given rotation.
	// @function Ray
	// @tparam Vec3 pos World position.
	// @tparam int roomNumber Room number.
	// @tparam Rotation rot Rotation according to which the input relative offset is rotated.
	// @tparam Vec3 relOffset Relative offset to cast.
	// @tparam[opt=false] bool collideMoveables Specfies if Ray should collide with moveables. Enabling this option is computationally expensive. Use only when necessary.
	// @tparam[opt=false] bool collideStatics Specfies if Ray should collide with statics. Enabling this option is computationally expensive. Use only when necessary.
	// @treturn Ray A new Ray.
	Ray::Ray(const Vec3& origin, int roomNumber, const Rotation& rot, const Vec3& relOffset)
	{
		auto target = Geometry::TranslatePoint(origin.ToVector3(), rot.ToEulerAngles(), relOffset.ToVector3());
		float dist = Vector3::Distance(origin.ToVector3(), target);

		auto dir = target - origin.ToVector3();
		dir.Normalize();

		_origin = origin;
		_direction = dir;
		_distance = dist;
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, false, false, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Rotation& rot, const Vec3& relOffset, bool collideMoveables)
	{
		auto target = Geometry::TranslatePoint(origin.ToVector3(), rot.ToEulerAngles(), relOffset.ToVector3());
		float dist = Vector3::Distance(origin.ToVector3(), target);

		auto dir = target - origin.ToVector3();
		dir.Normalize();

		_origin = origin;
		_direction = dir;
		_distance = dist;
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, collideMoveables, false, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Rotation& rot, const Vec3& relOffset, bool collideMoveables, bool collideStatics)
	{
		auto target = Geometry::TranslatePoint(origin.ToVector3(), rot.ToEulerAngles(), relOffset.ToVector3());
		float dist = Vector3::Distance(origin.ToVector3(), target);

		auto dir = target - origin.ToVector3();
		dir.Normalize();

		_origin = origin;
		_direction = dir;
		_distance = dist;
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, collideMoveables, false, collideStatics);
	}

	/// Get the Room hit by the Ray, if it intersects room geometry.
	// @function Ray:GetRoom
	// @treturn Room Room object.
	sol::optional<std::unique_ptr<Room>> Ray::GetRoom()
	{
		if (!_RayCollisionData.Room.IsIntersected)
			return sol::nullopt;

		int roomNumber = _RayCollisionData.Room.RoomNumber;
		return std::make_unique<Room>(g_Level.Rooms[roomNumber]);
	}

	/// Get the position at which the Ray intersects room geometry, if any.
	// @function Ray:GetRoomPosition
	// @treturn Vec3 World position.
	sol::optional<Vec3> Ray::GetRoomPosition()
	{
		return _RayCollisionData.Room.Position;
	}

	///Get the name of the room hit by the Ray, if any.
	// @functionRay:GetRoomName
	// @treturn string Room name.
	sol::optional<std::string> Ray::GetRoomName()
	{
		int roomNumber = _RayCollisionData.Room.RoomNumber;
		const auto& room = g_Level.Rooms[roomNumber];

		return room.Name;
	}

	///Get the number of the room hit by the Ray, if any.
	// @function Ray:GetRoomNumber
	// @treturn int Room number.
	sol::optional<int> Ray::GetRoomNumber()
	{
		return _RayCollisionData.Room.RoomNumber;
	}

	///Get the distance from the Ray origin to the point where it intersects room geometry, if any.
	// @function Ray:GetRoomNumber
	// @treturn int Room distance.
	sol::optional<float> Ray::GetRoomDistance()
	{
		return _RayCollisionData.Room.Distance;
	}

	///Get the Moveable hit by the Ray, if moveable collision is enabled and a hit occurred.
	// @function Ray:GetMoveable
	// @treturn Moveable Hit Moveable object.
	sol::optional <std::unique_ptr<Moveable>> Ray::GetMoveable()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		auto item = _RayCollisionData.Items.front().Item;
		return std::make_unique<Moveable>(item->Index);;
	}

	/// Get the position at which the Ray intersects a moveable, if moveable testing is enabled and a hit occurred.
	// @function Ray:GetMoveablePosition
	// @treturn Vec3 World position.
	sol::optional<Vec3> Ray::GetMoveablePosition()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		return _RayCollisionData.Items.front().Position;
	}

	///Get the distance from the Ray origin to the hit moveable, if moveable testing is enabled and a hit occurred.
	// @function Ray:GetMoveableDistance
	// @treturn int Distance from origin to hit moveable.
	sol::optional<float> Ray::GetMoveableDistance()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		return _RayCollisionData.Items.front().Distance;
	}

	///Get the Static hit by the Ray, if static testing is enabled and a hit occurred.
	// @function Ray:GetStatic
	// @treturn Static Hit Static object.
	sol::optional<std::unique_ptr<Static>> Ray::GetStatic()
	{	
		if (_RayCollisionData.Statics.empty())
			return sol::nullopt;

		auto* mesh = _RayCollisionData.Statics.front().Static;
		if (!mesh)
		return sol::nullopt;

		return std::make_unique<Static>(*mesh);
	}

	///Get the position at which the Ray intersects a static, if static testing is enabled and a hit occurred.
	// @function Ray:GetStaticPosition
	// @treturn Vec3 World position.
	sol::optional<Vec3> Ray::GetStaticPosition()
	{
		if (_RayCollisionData.Statics.empty())
			return sol::nullopt;

		return _RayCollisionData.Statics.front().Position;
	}

	///Get the distance from the Ray origin to the hit static, if static testing is enabled and a hit occurred.
	// @function Ray:GetStaticDistance
	// @treturn int Distance from origin to hit static.
	sol::optional<float> Ray::GetStaticDistance()
	{
		if (_RayCollisionData.Statics.empty())
			return sol::nullopt;

		return _RayCollisionData.Statics.front().Distance;
	}

	///Returns true if the Ray intersects room geometry.
	// If a room name is provided, returns true only when the intersection occurs with that room.
	// @function Ray:HitRoom
	// @tparam[opt] string roomName Name of the room to test.
	// @treturn bool True if a matching room was hit; false otherwise.
	bool Ray::HitRoom(TypeOrNil<std::string> roomName)
	{
		if (!_RayCollisionData.Room.IsIntersected)
			return false;

		std::string convertedString = ValueOr<std::string>(roomName, "");

		if (convertedString.empty())
			return true;

		int roomNumber = _RayCollisionData.Room.RoomNumber;
		const auto& room = g_Level.Rooms[roomNumber];

		return room.Name == convertedString;
	}

	///Returns true if the Ray intersects a Moveable object, if Moveable testing is enabled.
	// If a Moveable name is provided, returns true only when the intersection occurs with that Moveable.
	// @function Ray:HitMoveable
	// @tparam[opt] string moveableName Name of the Moveable to test.
	// @treturn bool True if a matching Moveable was hit; false otherwise.
	bool Ray::HitMoveable(TypeOrNil<std::string> moveableName)
	{
		if (_RayCollisionData.Items.empty())
			return false;

		std::string convertedString = ValueOr<std::string>(moveableName, "");

		if (convertedString.empty())
			return true;

		const auto& hit = _RayCollisionData.Items.front();
		return hit.Item && hit.Item->Name == convertedString;
	}

	///Returns true if the Ray intersects a Static object, if Static testing is enabled.
	// If a Static name is provided, returns true only when the intersection occurs with that Static.
	// @function Ray:HitStatic
	// @tparam[opt] string staticName Name of the Static to test.
	// @treturn bool True if a matching Static was hit; false otherwise.
	bool Ray::HitStatic(TypeOrNil<std::string> staticName)
	{
		if (_RayCollisionData.Statics.empty())
			return false;

		std::string convertedString = ValueOr<std::string>(staticName, "");

		if (convertedString.empty())
			return true;

		const auto& hit = _RayCollisionData.Statics.front();
		return hit.Static && hit.Static->Name == convertedString;
	}

	/// Preview thisRay in the Collision Stats debug page.
	// @functionRay:Preview
	void Ray::Preview()
	{
		constexpr auto TARGET_RADIUS = BLOCK(0.08f);
		constexpr auto COLOR = Color(1.0f, 1.0f, 0.8f, 0.2f);
		constexpr auto DEBUG_PAGE = RendererDebugPage::CollisionStats;
		
		auto dir = _direction;
		float dist = _distance;

		auto convertedPos = Vector3i(_origin);
		short roomNumber = FindRoomNumber(convertedPos);

		auto origin = _origin;
		auto target = Geometry::TranslatePoint(origin, dir, dist);
		auto los = GetLosCollision(origin, roomNumber, dir, dist, true, true, true);
		float closestDist = los.Room.Distance;
		target = los.Room.Position;

		for (const auto& movLos : los.Items)
		{
			if (movLos.Item->ObjectNumber == ID_LARA)
				continue;

			if (movLos.Distance < closestDist)
			{
				closestDist = movLos.Distance;
				target = movLos.Position;
				break;
			}
		}

		for (const auto& staticLos : los.Statics)
		{
			if (staticLos.Distance < closestDist)
			{
				closestDist = staticLos.Distance;
				target = staticLos.Position;
				break;
			}
		}

		DrawDebugLine(origin, target, COLOR, DEBUG_PAGE);
		DrawDebugTarget(target, Quaternion::Identity, TARGET_RADIUS, COLOR, DEBUG_PAGE);

	}
}
