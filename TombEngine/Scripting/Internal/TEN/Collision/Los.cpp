#include "framework.h"
#include "Scripting/Internal/TEN/Collision/Los.h"

#include "Game/collision/Los.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Collision/MaterialTypes.h"
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
	/// Casts a collision ray in the game world.
	// Provides collision information for collided rooms, moveables, and statics.
	//
	// @tenclass Collision.Ray
	// @pragma nostrip

	void Ray::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			Ray(const Vec3&, int, const Vec3&, float),
			Ray(const Vec3&, int, const Vec3&, float, ScriptIntersectionType),
			Ray(const Vec3&, int, const Vec3&, float, ScriptIntersectionType, ScriptIntersectionType),
			Ray(const Vec3&, int, const Vec3&, float, ScriptIntersectionType, ScriptIntersectionType, bool)>;

		// Register type.
		parent.new_usertype<Ray>(
			ScriptReserved_Ray,
			ctors(), sol::call_constructor, ctors(),

			// Getters
			ScriptReserved_RayGetRoom, &Ray::GetRoom,
			ScriptReserved_RayGetRoomPosition, &Ray::GetRoomPosition,
			ScriptReserved_RayGetRoomDistance, &Ray::GetRoomDistance,
			ScriptReserved_RayGetRoomNormal, &Ray::GetRoomNormal,
			ScriptReserved_RayGetMoveable, &Ray::GetMoveable,
			ScriptReserved_RayGetMoveablePosition, &Ray::GetMoveablePosition,
			ScriptReserved_RayGetMoveableDistance, &Ray::GetMoveableDistance,
			ScriptReserved_RayGetStatic, &Ray::GetStatic,
			ScriptReserved_RayGetStaticPosition, &Ray::GetStaticPosition,
			ScriptReserved_RayGetStaticDistance, &Ray::GetStaticDistance,

			ScriptReserved_RayGetMoveables, &Ray::GetMoveables,
			ScriptReserved_RayGetStatics, &Ray::GetStatics,

			// Inquirers
			ScriptReserved_RayHitMoveable, &Ray::HitMoveable,
			ScriptReserved_RayHitStatic, &Ray::HitStatic,
			ScriptReserved_RayHitRoom, &Ray::HitRoom,
			
			// Utilities
			ScriptReserved_ProbePreview, &Ray::Preview);
	}

	/// Create a ray at a specified world position, direction, and distance in a room.
	// @function Ray
	// @tparam Vec3 pos World position.
	// @tparam int roomNumber Origin room number.
	// @tparam Vec3 direction Direction vector.
	// @tparam float dist Maximum distance the ray can travel.
	// @tparam[opt=Collision.IntersectionType.BOX] Collision.IntersectionType hitMoveables Collide with moveables. Disable when not needed or required to optimize performance.
	// @tparam[opt=Collision.IntersectionType.BOX] Collision.IntersectionType hitStatics Collide with static meshes. Disable when not needed or required to optimize performance.
	// @tparam[opt=false] bool penetrate Continue the ray test after the first hit. Enable this when you need to collect all collision information beyond the first occlusion point.
	// @treturn Ray A new ray object.
	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist) :
		Ray(origin, roomNumber, dir, dist, ScriptIntersectionType::Box, ScriptIntersectionType::Box, false) { }

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, ScriptIntersectionType hitMoveables) :
		Ray(origin, roomNumber, dir, dist, hitMoveables, ScriptIntersectionType::Box, false) { }

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, ScriptIntersectionType hitMoveables, ScriptIntersectionType hitStatics) :
		Ray(origin, roomNumber, dir, dist, hitMoveables, hitStatics, false) { }

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, ScriptIntersectionType hitMoveables, ScriptIntersectionType hitStatics, bool penetrate)
	{
		bool collideItemBoxes = hitMoveables != ScriptIntersectionType::None;
		bool collideItemSpheres = hitMoveables == ScriptIntersectionType::BoxAndSphere;
		bool collideStaticBoxes = hitStatics != ScriptIntersectionType::None;

		if (hitStatics == ScriptIntersectionType::BoxAndSphere)
			TENLog("Ray collision with static mesh spheres is not supported at the moment. Using IntersectionType.BOX instead.", LogLevel::Warning);

		_los = GetLosCollision(origin, roomNumber, dir, dist, collideItemBoxes, collideItemSpheres, collideStaticBoxes);
		_origin = origin;
		_direction = dir;
		_distance = dist;
		_penetrate = penetrate;
	}

	bool Ray::IsOccluded(float dist) const
	{
		if (_penetrate)
			return false;

		if (_los.Room.IsIntersected && _los.Room.Distance < dist)
			return true;

		if (!_los.Items.empty() && _los.Items.front().Distance < dist)
			return true;

		if (!_los.Statics.empty() && _los.Statics.front().Distance < dist)
			return true;

		return false;
	}
	
	/// Get the room hit by the ray.
	// @function Ray:GetRoom
	// @treturn Objects.Room Room object. _nil: no room was hit._
	sol::optional<std::unique_ptr<Room>> Ray::GetRoom()
	{
		if (!_los.Room.IsIntersected || IsOccluded(_los.Room.Distance))
			return sol::nullopt;

		int roomNumber = _los.Room.RoomNumber;
		return std::make_unique<Room>(g_Level.Rooms[roomNumber]);
	}

	/// Get the position of the room geometry where the ray hit.
	// @function Ray:GetRoomPosition
	// @treturn Vec3 Hit position. _nil: no room was hit._
	sol::optional<Vec3> Ray::GetRoomPosition()
	{
		if (!_los.Room.IsIntersected || IsOccluded(_los.Room.Distance))
			return sol::nullopt;

		return _los.Room.Position;
	}

	/// Get the distance from the ray origin point to the room hit position.
	// @function Ray:GetRoomDistance
	// @treturn float Hit distance. _nil: no room was hit._
	sol::optional<float> Ray::GetRoomDistance()
	{
		if (!_los.Room.IsIntersected || IsOccluded(_los.Room.Distance))
			return sol::nullopt;

		return _los.Room.Distance;
	}

	/// Get the surface normal of the room geometry where the ray hit.
	// @function Ray:GetRoomNormal
	// @treturn Vec3 Surface normal. _nil: no room was hit, or no valid room geometry was hit._
	sol::optional<Vec3> Ray::GetRoomNormal()
	{
		if (!_los.Room.IsIntersected || IsOccluded(_los.Room.Distance) || !_los.Room.Triangle.has_value())
			return sol::nullopt;

		return _los.Room.Triangle.value().Normal;
	}

	/// Get the first moveable hit by the ray.
	// Moveable hits will be detected only if `hitMoveables` setting was enabled while creating a ray.
	// @function Ray:GetMoveable
	// @treturn Objects.Moveable Moveable object. _nil: no moveable was hit._
	sol::optional<std::unique_ptr<Moveable>> Ray::GetMoveable()
	{
		if (_los.Items.empty() || IsOccluded(_los.Items.front().Distance))
			return sol::nullopt;

		auto mov = _los.Items.front().Item;
		return std::make_unique<Moveable>(mov->Index);
	}

	/// Gets all moveables hit by the ray.
	// Multiple moveable hits will be detected only if `hitMoveables` and `penetrate` settings were enabled while creating a ray.
	// @function Ray:GetMoveables
	// @treturn table Table of moveables hit by the ray. _nil: no moveables were hit._
	sol::optional<std::vector<std::unique_ptr<Moveable>>> Ray::GetMoveables()
	{
		if (_los.Items.empty() || IsOccluded(_los.Items.front().Distance))
			return sol::nullopt;

		std::vector<std::unique_ptr<Moveable>> moveables;
		moveables.reserve(_los.Items.size());

		for (const auto& item : _los.Items)
		{
			moveables.push_back(std::make_unique<Moveable>(item.Item->Index));
		}

		return moveables;
	}

	/// Get the position of the moveable where the ray hit.
	// Valid position will be returned only if `hitMoveables` setting was enabled while creating a ray.
	// @function Ray:GetMoveablePosition
	// @treturn Vec3 Hit position. _nil: no moveable was hit._
	sol::optional<Vec3> Ray::GetMoveablePosition()
	{
		if (_los.Items.empty() || IsOccluded(_los.Items.front().Distance))
			return sol::nullopt;

		return _los.Items.front().Position;
	}
	
	/// Get the distance from the ray origin to the first moveable hit position.
	// Valid distance will be returned only if `hitMoveables` setting was enabled while creating a ray.
	// @function Ray:GetMoveableDistance
	// @treturn float Hit distance. _nil: no moveable was hit._
	sol::optional<float> Ray::GetMoveableDistance()
	{
		if (_los.Items.empty() || IsOccluded(_los.Items.front().Distance))
			return sol::nullopt;

		return _los.Items.front().Distance;
	}

	/// Get the first static mesh hit by the ray.
	// Static mesh hits will be detected only if `hitStatics` setting was enabled while creating a ray.
	// @function Ray:GetStatic
	// @treturn Objects.Static Static mesh object. _nil: no static mesh was hit._
	sol::optional<std::unique_ptr<Static>> Ray::GetStatic()
	{	
		if (_los.Statics.empty() || IsOccluded(_los.Statics.front().Distance))
			return sol::nullopt;

		auto* staticObj = _los.Statics.front().Static;
		if (staticObj == nullptr)
			return sol::nullopt;

		return std::make_unique<Static>(*staticObj);
	}

	/// Gets all statics hit by the ray.
	// Multiple statics hits will be detected only if `hitStatics` and `penetrate` settings were enabled while creating a ray.
	// @function Ray:GetStatics
	// @treturn table Table of statics hit by the ray. _nil: no statics were hit._
	sol::optional<std::vector<std::unique_ptr<Static>>> Ray::GetStatics()
	{
		if (_los.Statics.empty() || IsOccluded(_los.Statics.front().Distance))
			return sol::nullopt;

		std::vector<std::unique_ptr<Static>> statics;
		statics.reserve(_los.Statics.size());

		for (const auto& item : _los.Statics)
		{
			auto* staticObj = item.Static;
			statics.push_back(std::make_unique<Static>(*staticObj));
		}

		return statics;
	}

	/// Get the position of the first static mesh hit by the ray.
	// Valid position will be returned only if `hitStatics` setting was enabled while creating a ray.
	// @function Ray:GetStaticPosition
	// @treturn Vec3 Hit position. _nil: no static mesh was hit._
	sol::optional<Vec3> Ray::GetStaticPosition()
	{
		if (_los.Statics.empty() || IsOccluded(_los.Statics.front().Distance))
			return sol::nullopt;

		return _los.Statics.front().Position;
	}

	/// Get the distance from the ray origin to the first static mesh hit position.
	// Valid distance will be returned only if `hitStatics` setting was enabled while creating a ray.
	// @function Ray:GetStaticDistance
	// @treturn float Hit distance. _nil: no static mesh was hit._
	sol::optional<float> Ray::GetStaticDistance()
	{
		if (_los.Statics.empty() || IsOccluded(_los.Statics.front().Distance))
			return sol::nullopt;

		return _los.Statics.front().Distance;
	}

	/// Check if the ray hit a room.
	// If a room name is provided, returns `true` only when the hit occurs with the relevant room geometry.
	// @function Ray:HitRoom
	// @tparam[opt] string name Name of the room to check for.
	// @treturn bool Room hit status. _true if a room was hit, false otherwise._
	bool Ray::HitRoom(const TypeOrNil<std::string>& name)
	{
		if (!_los.Room.IsIntersected || IsOccluded(_los.Room.Distance))
			return false;

		auto convertedString = ValueOr<std::string>(name, {});
		if (convertedString.empty())
			return true;

		const auto& room = g_Level.Rooms[_los.Room.RoomNumber];
		return room.Name == convertedString;
	}

	/// Check if the ray hit a moveable.
	// Moveable hits will be detected only if `hitMoveables` setting was enabled while creating a ray.
	// If a moveable name is provided, returns `true` only when the hit occurs with the relevant moveable.
	// @function Ray:HitMoveable
	// @tparam[opt] string name Name of the moveable to check for.
	// @treturn bool Moveable hit status. _true if a moveable was hit, false otherwise._
	bool Ray::HitMoveable(const TypeOrNil<std::string>& name)
	{
		if (_los.Items.empty() || IsOccluded(_los.Items.front().Distance))
			return false;

		auto searchName = ValueOr<std::string>(name, {});
		if (searchName.empty())
			return true;

		for (const auto& itemLos : _los.Items)
		{
			if (itemLos.Item != nullptr && itemLos.Item->Name == searchName)
				return true;
		}

		return false;
	}

	/// Check if the ray hit a static mesh.
	// Static mesh hits will be detected only if `hitStatics` setting was enabled while creating a ray.
	// If a static mesh name is provided, returns `true` only when the hit occurs with the relevant static mesh.
	// @function Ray:HitStatic
	// @tparam[opt] string name Name of the static mesh to check for.
	// @treturn bool Static mesh hit status. _true if a static mesh was hit, false otherwise._
	bool Ray::HitStatic(const TypeOrNil<std::string>& name)
	{
		if (_los.Statics.empty() || IsOccluded(_los.Statics.front().Distance))
			return false;

		auto searchName = ValueOr<std::string>(name, {});
		if (searchName.empty())
			return true;

		for (const auto& staticLos : _los.Statics)
		{
			if (staticLos.Static != nullptr && staticLos.Static->Name == searchName)
				return true;
		}

		return false;
	}

	/// Preview this ray in the Collision Stats debug page.
	// @function Ray:Preview
	void Ray::Preview()
	{
		constexpr int  TARGET_RADIUS = BLOCK(0.08f);
		constexpr auto HIT_COLOR     = Color(1.0f, 0.5f, 0.5f, 0.3f);
		constexpr auto MISS_COLOR    = Color(0.5f, 1.0f, 0.5f, 0.3f);
		
		auto target = _los.Room.Position;

		// Draw moveable targets.
		for (const auto& movLos : _los.Items)
		{
			// Skip player.
			if (movLos.Item->ObjectNumber == ID_LARA)
				continue;

			// Clip all further moveables if occlusion happened.
			if (IsOccluded(movLos.Distance))
				break;

			DrawDebugTarget(movLos.Position, Quaternion::Identity, TARGET_RADIUS, HIT_COLOR, RendererDebugPage::CollisionStats);
			target = movLos.Position;
		}

		// Draw static mesh targets.
		for (const auto& staticLos : _los.Statics)
		{
			// Clip all further static meshes if occlusion happened.
			if (IsOccluded(staticLos.Distance))
				break;

			DrawDebugTarget(staticLos.Position, Quaternion::Identity, TARGET_RADIUS, HIT_COLOR, RendererDebugPage::CollisionStats);
			target = staticLos.Position;
		}

		bool isIntersected = _los.Room.IsIntersected || target != _los.Room.Position;
		auto lineColor = isIntersected ? HIT_COLOR : MISS_COLOR;
		
		// Draw room geometry target.
		if (!IsOccluded(_los.Room.Distance))
		{
			DrawDebugTarget(_los.Room.Position, Quaternion::Identity, TARGET_RADIUS, lineColor, RendererDebugPage::CollisionStats);
			target = _los.Room.Position;
		}

		// Draw ray.
		DrawDebugLine(_origin, target, lineColor, RendererDebugPage::CollisionStats);
	}
}
