#include "framework.h"
#include "Scripting/Internal/TEN/Objects/Creature/Creature.h"
#include "Scripting/Internal/TEN/Objects/Creature/CreatureStates.h"

#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/level.h"
#include <Game/misc.h>

namespace TEN::Scripting::Objects
{
	/// Represents the AI and behavior state of a creature in the game.
	//
	// @tenclass Objects.CreatureInfo
	// @pragma nostrip

	void LuaCreatureInfo::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			LuaCreatureInfo(const Moveable& mov)>;

		// Register type.
		parent.new_usertype<LuaCreatureInfo>(ScriptReserved_CreatureInfo,
			ctors(), sol::call_constructor, ctors(),

			// Getters
			ScriptReserved_GetMood, &LuaCreatureInfo::GetMood,
			ScriptReserved_GetCreatureTarget, &LuaCreatureInfo::GetTarget,
			ScriptReserved_GetTargetPosition, &LuaCreatureInfo::GetTargetPosition,
			ScriptReserved_SetCreatureTarget, & LuaCreatureInfo::SetTarget,
			ScriptReserved_SetTargetPosition, & LuaCreatureInfo::SetTargetPosition,
			ScriptReserved_ClearTarget, & LuaCreatureInfo::ClearTarget,
			ScriptReserved_IsAlerted, & LuaCreatureInfo::IsAlerted,
			ScriptReserved_IsFriendly, & LuaCreatureInfo::IsFriendly,
			ScriptReserved_IsHurtByPlayer, & LuaCreatureInfo::IsHurtByPlayer,
			ScriptReserved_IsPoisoned, & LuaCreatureInfo::IsPoisoned,
			ScriptReserved_IsAtGoal, & LuaCreatureInfo::IsAtGoal);
			
	}

	/// Create creature info for the provided moveable.
	// @function CreatureInfo
	// @tparam moveable mov Moveable object to probe creature info. Must be an active enemy.
	// @treturn CreatureInfo Creature info for the moveable.
	LuaCreatureInfo::LuaCreatureInfo(const Moveable& mov)
	{
			auto* item = &g_Level.Items[mov.GetIndex()];
			
			if (!CreatureActive(mov.GetIndex()))
				return;

			if (item->IsCreature())
			m_Creature = GetCreatureInfo(item);
	}

	/// Gets the current mood of the creature.
	// @function GetMood
	// @treturn Objects.CreatureMood The current mood of the creature.
	MoodType LuaCreatureInfo::GetMood()
	{
		return m_Creature->Mood;
	}

	/// Gets the current target of the creature.
	// @function GetTarget
	// @treturn moveable The moveable object representing the target, or null if no target is set.
	std::optional<Moveable> LuaCreatureInfo::GetTarget()
	{
		auto enemy = m_Creature->Enemy;
		return Moveable(enemy->Index);
	}

	/// Gets the current target position of the creature.
	// @function GetTargetPosition
	// @treturn Vec3 The position of the creature's target.
	Vec3 LuaCreatureInfo::GetTargetPosition()
	{

		return m_Creature->Target;
	}

	/// Sets a new target for the creature.
	// @function SetTarget
	// @tparam moveable mov The moveable object to set as the target.
	void LuaCreatureInfo::SetTarget(Moveable& mov)
	{
		auto* item = &g_Level.Items[mov.GetIndex()];
		m_Creature->Enemy = item;
	}

	/// Sets the position of the creature's target.
	// @function SetTargetPosition
	// @tparam Vec3 position The target position to set.
	void LuaCreatureInfo::SetTargetPosition(Vec3& position)
	{
		m_Creature->Target = position.ToVector3i();
	}

	/// Clears the current target of the creature.
	// @function ClearTarget
	void LuaCreatureInfo::ClearTarget()
	{
		m_Creature->Enemy = nullptr;
	}

	/// Checks if the creature is in an alerted state.
	// @function IsAlerted
	// @treturn bool Creature alert state. __true: if the creature is alerted, false: not alerted__
	bool LuaCreatureInfo::IsAlerted()
	{
		return m_Creature->Alerted;
	}

	/// Checks if the creature is friendly. Only returns true for friendly creatures like monks (TR2) or troops (TR4).
	// @function IsFriendly
	// @treturn Creature friendly status. bool __true: if the creature is friendly, false: not friendly__
	bool LuaCreatureInfo::IsFriendly()
	{
		return m_Creature->Friendly;
	}

	/// Checks if the creature has been hurt by player.
	// @function IsHurtByPlayer
	// @treturn bool Creature hit status. __true: is hit, false: isn't hit__
	bool LuaCreatureInfo::IsHurtByPlayer()
	{
		return m_Creature->HurtByLara;
	}

	/// Checks if the creature is poisoned.
	// @function IsPoisoned
	// @treturn bool Creature poison status. __true: is poisoned, false: isn't poisoned__
	bool LuaCreatureInfo::IsPoisoned()
	{
		return m_Creature->Poisoned;
	}

	/// Checks if the creature has reached its goal.
	// @function IsAtGoal
	// @treturn bool Creature position status. __true: is at its goal, false: isn't at its goal__.
	bool LuaCreatureInfo::IsAtGoal()
	{
		return m_Creature->ReachedGoal;
	}
}
