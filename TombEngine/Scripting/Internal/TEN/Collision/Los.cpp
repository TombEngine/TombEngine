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
	/// Casts a collision Ray in the game world.
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
			ScriptReserved_RayGetMoveables, &Ray::GetMoveables,
			ScriptReserved_RayGetMoveablePosition, &Ray::GetMoveablePosition,
			ScriptReserved_RayGetMoveableDistance, &Ray::GetMoveableDistance,
			ScriptReserved_RayGetStatic, &Ray::GetStatic,
			ScriptReserved_RayGetStaticPosition, &Ray::GetStaticPosition,
			ScriptReserved_RayGetStaticDistance, &Ray::GetStaticDistance,

			// Inquirers
			ScriptReserved_RayHitMoveable, &Ray::HitMoveable,
			ScriptReserved_RayHitStatic, &Ray::HitStatic,
			ScriptReserved_RayHitRoom, &Ray::HitRoom,
			
			// Utilities
			ScriptReserved_ProbePreview, &Ray::Preview);
	}

	/// Create a Ray at a specified world position, direction, and distance in a room.
	// @function Ray
	// @tparam Vec3 pos World position.
	// @tparam int roomNumber Origin room number.
	// @tparam Vec3 direction Direction vector.
	// @tparam float dist Maximum distance the ray can travel.
	// @tparam[opt=Collision.IntersectionType.BOX] Collision.IntersectionType hitMoveables Collide with moveables. Disable when not needed or required to optimize performance.
	// @tparam[opt=Collision.IntersectionType.BOX] Collision.IntersectionType hitStatics Collide with statics. Disable when not needed or required to optimize performance.
	// @tparam[opt=false] bool penetrate Continue the ray test after the first hit. Enable this when you need to collect all collision information beyond the first occlusion point.
	// @treturn Ray A new Ray.
	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist)
	{
		*this = Ray(origin, roomNumber, dir, dist, ScriptIntersectionType::Box, ScriptIntersectionType::Box, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, ScriptIntersectionType hitMoveables)
	{
		*this = Ray(origin, roomNumber, dir, dist, hitMoveables, ScriptIntersectionType::Box, false);
	}

	Ray::Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist, ScriptIntersectionType hitMoveables, ScriptIntersectionType hitStatics)
	{
		*this = Ray(origin, roomNumber, dir, dist, hitMoveables, hitStatics, false);
	}

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
	
	/// Get the Room hit by the Ray.
	// @function Ray:GetRoom
	// @treturn Objects.Room Room object. __nil: no Room was hit.__
	sol::optional<std::unique_ptr<Room>> Ray::GetRoom()
	{
		if (!_los.Room.IsIntersected || IsOccluded(_los.Room.Distance))
			return sol::nullopt;

		int roomNumber = _los.Room.RoomNumber;
		return std::make_unique<Room>(g_Level.Rooms[roomNumber]);
	}

	/// Get the position of the Room hit by the Ray.
	// @function Ray:GetRoomPosition
	// @treturn Vec3 Hit position. __nil: no Room was hit.__
	sol::optional<Vec3> Ray::GetRoomPosition()
	{
		if (!_los.Room.IsIntersected || IsOccluded(_los.Room.Distance))
			return sol::nullopt;

		return _los.Room.Position;
	}

	/// Get the distance from the Ray origin to the Room hit position.
	// @function Ray:GetRoomNumber
	// @treturn float Hit distance. __nil: no Room was hit.__
	sol::optional<float> Ray::GetRoomDistance()
	{
		if (!_los.Room.IsIntersected || IsOccluded(_los.Room.Distance))
			return sol::nullopt;

		return _los.Room.Distance;
	}

	/// Get the surface normal of the Room geometry hit by the Ray.
	// @function Ray:GetRoomNormal
	// @treturn Vec3 Surface normal. __nil: no Room was hit, or no valid Room geometry was hit.__
	sol::optional<Vec3> Ray::GetRoomNormal()
	{
		if (!_los.Room.IsIntersected || IsOccluded(_los.Room.Distance) || !_los.Room.Triangle.has_value())
			return sol::nullopt;

		return _los.Room.Triangle.value().Normal;
	}

	/// Get the first Moveable hit by the Ray.
	// Note: Valid Moveables are only possible if Moveable hits were enabled.
	// @function Ray:GetMoveable
	// @treturn Objects.Moveable Moveable object. __nil: no Moveable was hit.__
	sol::optional<std::unique_ptr<Moveable>> Ray::GetMoveable()
	{
		if (_los.Items.empty() || IsOccluded(_los.Items.front().Distance))
			return sol::nullopt;

		auto mov = _los.Items.front().Item;
		return std::make_unique<Moveable>(mov->Index);
	}

	/// Gets all the Moveables hit by the Ray.
	// Note: Valid Moveables are only possible if Moveable hits were enabled.
	// @function Ray:GetMoveables
	// @treturn table Table of moveables hit by the Ray. __nil: no Moveable was hit.__
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

	/// Get the position of the first Moveable hit by the Ray.
	// Note: Valid positions are only possible if Moveable hits were enabled.
	// @function Ray:GetMoveablePosition
	// @treturn Vec3 Hit position. __nil: no Moveable was hit.__
	sol::optional<Vec3> Ray::GetMoveablePosition()
	{
		if (_los.Items.empty() || IsOccluded(_los.Items.front().Distance))
			return sol::nullopt;

		return _los.Items.front().Position;
	}
	
	/// Get the distance from the Ray origin to the first Moveable hit position.
	// Note: Valid distances are only possible if Moveable hits were enabled.
	// @function Ray:GetMoveableDistance
	// @treturn float Hit distance. __nil: no Moveable was hit.__
	sol::optional<float> Ray::GetMoveableDistance()
	{
		if (_los.Items.empty() || IsOccluded(_los.Items.front().Distance))
			return sol::nullopt;

		return _los.Items.front().Distance;
	}

	/// Get the Static hit by the Ray.
	// Note: Valid Statics are only possible if Moveable hits were enabled.
	// @function Ray:GetStatic
	// @treturn Objects.Static Static object. __nil: no Static was hit.__
	sol::optional<std::unique_ptr<Static>> Ray::GetStatic()
	{	
		if (_los.Statics.empty() || IsOccluded(_los.Statics.front().Distance))
			return sol::nullopt;

		auto* staticObj = _los.Statics.front().Static;
		if (staticObj == nullptr)
			return sol::nullopt;

		return std::make_unique<Static>(*staticObj);
	}

	/// Get the position of the first Static hit by the Ray.
	// Note: Valid positions are only possible if Static hits were enabled.
	// @function Ray:GetStaticPosition
	// @treturn Vec3 Hit position. __nil: no Static was hit.__
	sol::optional<Vec3> Ray::GetStaticPosition()
	{
		if (_los.Statics.empty() || IsOccluded(_los.Statics.front().Distance))
			return sol::nullopt;

		return _los.Statics.front().Position;
	}

	/// Get the distance from the Ray origin to the first Static hit position.
	// Note: Valid distances are only possible if Static hits were enabled.
	// @function Ray:GetStaticDistance
	// @treturn float Hit distance. __nil: no Static was hit.__
	sol::optional<float> Ray::GetStaticDistance()
	{
		if (_los.Statics.empty() || IsOccluded(_los.Statics.front().Distance))
			return sol::nullopt;

		return _los.Statics.front().Distance;
	}

	/// Check if the Ray hit a Room.
	// If a Room name is provided, returns true only when the hit occurs with the relevant Room.
	// @function Ray:HitRoom
	// @tparam[opt] string name Name of the room to check for.
	// @treturn bool True if a Room was hit, false otherwise.
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

	/// Check if the Ray hit a Moveable.
	// Note: Valid checks are only possible if Moveable hits were enabled.
	// If a Moveable name is provided, returns true only when the hit occurs with the relevant Moveable.
	// @function Ray:HitMoveable
	// @tparam[opt] string name Name of the Moveable to check for.
	// @treturn bool True if a Moveable was hit, false otherwise.
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

	/// Check if the Ray hit a Static.
	// Note: Valid checks are only possible if Static hits were enabled.
	// If a Static name is provided, returns true only when the hit occurs with the relevant Static.
	// @function Ray:HitStatic
	// @tparam[opt] string name Name of the Static to check for.
	// @treturn bool True if a Static was hit, false otherwise.
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

	/// Preview this Ray in the Collision Stats debug page.
	// @function Ray:Preview
	void Ray::Preview()
	{
		constexpr int  TARGET_RADIUS = BLOCK(0.08f);
		constexpr auto COLOR         = Color(1.0f, 1.0f, 0.8f, 0.2f);
		
		short roomNumber = FindRoomNumber(Vector3i(_origin));
		auto los = GetLosCollision(_origin, roomNumber, _direction, _distance, true, true, true);
		float closestDist = los.Room.Distance;
		auto target = los.Room.Position;

		// Clip moveable.
		for (const auto& movLos : los.Items)
		{
			// Skip player.
			if (movLos.Item->ObjectNumber == ID_LARA)
				continue;

			// Clip non-player moveable if it exists.
			if (movLos.Distance < closestDist)
			{
				closestDist = movLos.Distance;
				target = movLos.Position;
			}

			break;
		}

		// Clip static.
		if (!los.Statics.empty())
		{
			const auto& staticLos = los.Statics.front();
			if (staticLos.Distance < closestDist)
			{
				closestDist = staticLos.Distance;
				target = staticLos.Position;
			}
		}

		DrawDebugLine(_origin, target, COLOR, RendererDebugPage::CollisionStats);
		DrawDebugTarget(target, Quaternion::Identity, TARGET_RADIUS, COLOR, RendererDebugPage::CollisionStats);
	}
}
