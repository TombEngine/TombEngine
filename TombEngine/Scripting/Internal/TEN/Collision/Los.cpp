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

namespace TEN::Scripting::Collision
{
	/// Represents a collisionRay in the game world.
	// Provides collision information from a reference world position.
	//
	// @tenclass Collision.Los
	// @pragma nostrip

	void Ray::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist,
				bool collideItems, bool collideSpheres, bool collideStatics)>;

		// Register type.
		parent.new_usertype<Ray>(
			ScriptReserved_Ray,
			ctors(), sol::call_constructor, ctors(),

			// Getters
			ScriptReserved_RayGetHitRoom, &Ray::GetHitRoom,
			ScriptReserved_RayGetHitPosition, &Ray::GetHitPosition,
			ScriptReserved_RayGetHitRoomName, &Ray::GetHitRoomName,
			ScriptReserved_RayGetHitRoomNumber, &Ray::GetHitRoomNumber,
			ScriptReserved_RayGetHitRoomDistance, &Ray::GetHitRoomDistance,
			ScriptReserved_RayGetHitMoveable, &Ray::GetHitMoveable,
			ScriptReserved_RayGetHitMoveablePosition, & Ray::GetHitMoveablePosition,
			ScriptReserved_RayGetHitMoveableRoomName, & Ray::GetHitMoveableRoomName,
			ScriptReserved_RayGetHitMoveableRoomNumber, &Ray::GetHitMoveableRoomNumber,
			ScriptReserved_RayGetHitMoveableDistance, &Ray::GetHitMoveableDistance,
			ScriptReserved_RayGetHitStatic, &Ray::GetHitStatic,
			ScriptReserved_RayGetHitStaticPosition, & Ray::GetHitStaticPosition,
			ScriptReserved_RayGetHitStaticRoomName, & Ray::GetHitStaticRoomName,
			ScriptReserved_RayGetHitStaticRoomNumber, &Ray::GetHitStaticRoomNumber,
			ScriptReserved_RayGetHitStaticDistance, & Ray::GetHitStaticDistance,

			// Inquirers
			ScriptReserved_RayHitMoveable, &Ray::HitMoveable,
			ScriptReserved_RayHitStatic, &Ray::HitStatic,
			ScriptReserved_RayHitRoom, &Ray::HitRoom,
			
			// Utilities
			ScriptReserved_ProbePreview, &Ray::Preview);
	}

	/// Create a Ray at a specified world position in a room.
	// @functionRay
	// @tparam Vec3 pos World position.
	// @tparam[opt] int roomNumber Room number. Must be used if probing a position in an overlapping room.
	// @treturnRay A newRay.
	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist,
		bool collideItems, bool collideSpheres, bool collideStatics)
	{
		/*auto convertedPos = pos.ToVector3i();
		_LosCollisionData = GetPointCollision(convertedPos, FindRoomNumber(convertedPos));*/

		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, collideItems, collideSpheres, collideStatics);
	}

	/// Get the Room object of this Ray.
	// @functionRay:GetRoom
	// @treturn Room Room object.
	std::unique_ptr<Room> Ray::GetHitRoom()
	{
		int roomNumber = _RayCollisionData.Room.RoomNumber;
		return std::make_unique<Room>(g_Level.Rooms[roomNumber]);
	}

	sol::optional<Vec3> Ray::GetHitPosition()
	{
		return sol::optional<Vec3>();
	}

	/// Get the room name of thisRay.
	// @functionRay:GetRoomName
	// @treturn string Room name.
	sol::optional<std::string> Ray::GetHitRoomName()
	{
		int roomNumber = _RayCollisionData.Room.RoomNumber;
		const auto& room = g_Level.Rooms[roomNumber];

		return room.Name;
	}

	sol::optional<int> Ray::GetHitRoomNumber()
	{
		return _RayCollisionData.Room.RoomNumber;
	}

	sol::optional<float> Ray::GetHitRoomDistance()
	{
		return _RayCollisionData.Room.Distance;
	}

	std::unique_ptr<Moveable> Ray::GetHitMoveable()
	{
		return _RayCollisionData.Items.front().Item;
	}

	sol::optional<Vec3> Ray::GetHitMoveablePosition()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		return _RayCollisionData.Items.front().Position;
	}

	sol::optional<std::string> Ray::GetHitMoveableRoomName()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		int roomNumber = _RayCollisionData.Items.front().RoomNumber;
		const auto& room = g_Level.Rooms[roomNumber];

		return room.Name;
	}

	sol::optional<int> Ray::GetHitMoveableRoomNumber()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		return _RayCollisionData.Items.front().RoomNumber;
	}

	sol::optional<float> Ray::GetHitMoveableDistance()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		return _RayCollisionData.Items.front().Distance;
	}

	std::unique_ptr<Static> Ray::GetHitStatic()
	{
		return _RayCollisionData.Statics.front().Static;
	}

	sol::optional<Vec3> Ray::GetHitStaticPosition()
	{
		if (_RayCollisionData.Statics.empty())
			return sol::nullopt;

		return _RayCollisionData.Statics.front().Position;
	}

	sol::optional<std::string> Ray::GetHitStaticRoomName()
	{
		if (_RayCollisionData.Statics.empty())
			return sol::nullopt;

		int roomNumber = _RayCollisionData.Statics.front().RoomNumber;
		const auto& room = g_Level.Rooms[roomNumber];

		return room.Name;
	}

	sol::optional<int> Ray::GetHitStaticRoomNumber()
	{
		if (_RayCollisionData.Statics.empty())
			return sol::nullopt;

		return _RayCollisionData.Statics.front().RoomNumber;
	}

	sol::optional<float> Ray::GetHitStaticDistance()
	{
		if (_RayCollisionData.Statics.empty())
			return sol::nullopt;

		return _RayCollisionData.Statics.front().Distance;
	}

	sol::optional<bool> Ray::HitRoom()
	{
		return _RayCollisionData.Room.IsIntersected;
	}

	sol::optional<bool> Ray::HitMoveable()
	{
		if (!_RayCollisionData.Items.empty())
			return true;

		return sol::nullopt;
	}

	sol::optional<bool> Ray::HitStatic()
	{
		if (!_RayCollisionData.Statics.empty())
			return true;

		return sol::nullopt;
	}

	/// Preview thisRay in the Collision Stats debug page.
	// @functionRay:Preview
	void Ray::Preview()
	{
	
	}

	void Register(sol::state* state, sol::table& parent)
	{
		auto collTable = sol::table(state->lua_state(), sol::create);
		parent.set(ScriptReserved_Ray, collTable);

		Ray::Register(collTable);
	}
}
