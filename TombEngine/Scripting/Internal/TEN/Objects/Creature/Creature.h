#pragma once

#include "Game/itemdata/creature_info.h"
#include "Scripting/Internal/ScriptUtil.h"

namespace sol { class state; };
namespace TEN::Scripting { class Rotation; }
class Moveable;
class Vec3;

namespace TEN::Scripting::Objects
{
    class LuaCreatureInfo
    {
    public:
        static void Register(sol::table& parent);

    private:
        // Fields

		CreatureInfo* m_Creature = nullptr;

    public:
        // Constructors
		LuaCreatureInfo() = default;
		LuaCreatureInfo(const Moveable& mov);
				
        // Getters
		MoodType					GetMood();
		std::optional<Moveable>		GetTarget();
		Vec3						GetTargetPosition();
		
		// Setters
		void SetTarget(Moveable& moveable);
		void SetTargetPosition(Vec3& position);
		void ClearTarget();

		// Inquirers
		bool				IsAlerted();
		bool				IsFriendly();
		bool				IsHurtByPlayer();
		bool				IsPoisoned();
		bool				IsAtGoal();

    };
}
