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
			Ray(const Vec3&, int, const Vec3&, float),
			Ray(const Vec3&, int, const Vec3&, float, bool),
			Ray(const Vec3&, int, const Vec3&, float, bool, bool)>;

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
			ScriptReserved_RayGetHitMoveableDistance, &Ray::GetHitMoveableDistance,
			ScriptReserved_RayGetHitStatic, &Ray::GetHitStatic,
			ScriptReserved_RayGetHitStaticPosition, & Ray::GetHitStaticPosition,
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

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist)
	{
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, false, false, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist,
		bool collideMoveables)
	{
		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, collideMoveables, false, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist,
		bool collideMoveables, bool collideStatics)
	{
		/*auto convertedPos = pos.ToVector3i();
		_LosCollisionData = GetPointCollision(convertedPos, FindRoomNumber(convertedPos));*/

		_RayCollisionData = GetLosCollision(origin, roomNumber, dir, dist, collideMoveables, false, collideStatics);
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

	sol::optional <std::unique_ptr<Moveable>> Ray::GetHitMoveable()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		auto item = _RayCollisionData.Items.front().Item;
		return std::make_unique<Moveable>(item->Index);;
	}

	sol::optional<Vec3> Ray::GetHitMoveablePosition()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		return _RayCollisionData.Items.front().Position;
	}

	sol::optional<float> Ray::GetHitMoveableDistance()
	{
		if (_RayCollisionData.Items.empty())
			return sol::nullopt;

		return _RayCollisionData.Items.front().Distance;
	}

	sol::optional<std::unique_ptr<Static>> Ray::GetHitStatic()
	{	
		//if (_RayCollisionData.Statics.empty())
		//	return sol::nullopt;

		//const auto* mesh = _RayCollisionData.Statics.front().Static;
		//if (!mesh)
		return sol::nullopt;

		//return std::make_unique<Static>(*mesh);
	}

	sol::optional<Vec3> Ray::GetHitStaticPosition()
	{
		if (_RayCollisionData.Statics.empty())
			return sol::nullopt;

		return _RayCollisionData.Statics.front().Position;
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
		constexpr auto TARGET_RADIUS = BLOCK(0.08f);
		constexpr auto COLOR = Color(1.0f, 1.0f, 0.8f, 0.2f);
		constexpr auto DEBUG_PAGE = RendererDebugPage::CollisionStats;
	
	}
}
