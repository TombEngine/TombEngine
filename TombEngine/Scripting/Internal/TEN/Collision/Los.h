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

    public:
        // Constructors

		Ray() = default;
		Ray(const Vec3& origin, int roomNumber, const Vec3& dir, float dist,
			bool collideItems, bool collideSpheres, bool collideStatics);

        // Getters

		std::unique_ptr<Room>		GetHitRoom();
		sol::optional<Vec3>			GetHitPosition();
		sol::optional<std::string>	GetHitRoomName();
		sol::optional<int>			GetHitRoomNumber();
		sol::optional<float>		GetHitRoomDistance();
		std::unique_ptr<Moveable>	GetHitMoveable();
		sol::optional<Vec3>			GetHitMoveablePosition();
		sol::optional<std::string>	GetHitMoveableRoomName();
		sol::optional<int>			GetHitMoveableRoomNumber();
		sol::optional<float>		GetHitMoveableDistance();
		std::unique_ptr<Static>		GetHitStatic();
		sol::optional<Vec3>			GetHitStaticPosition();
		sol::optional<std::string>	GetHitStaticRoomName();
		sol::optional<int>			GetHitStaticRoomNumber();
		sol::optional<float>		GetHitStaticDistance();

		// Inquirers

		sol::optional<bool> HitMoveable();
		sol::optional<bool> HitStatic();
		sol::optional<bool>	HitRoom();
		
		// Utilities

		void Preview();
    };

	void Register(sol::state* lua, sol::table& parent);
}
