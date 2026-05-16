#include "framework.h"
#include "Scripting/Internal/TEN/Objects/Creature/Creature.h"
#include "Scripting/Internal/TEN/Objects/Creature/CreatureStates.h"

#include "Game/misc.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

namespace TEN::Scripting::Objects
{
	/// A derivative of the @{Objects.Moveable} class that represents the AI and behavior state of an enemy creature in the game.
	//
	// @tenclass Objects.Creature
	// @pragma nostrip

	static auto IndexError = IndexErrorMaker(ScriptCreature, ScriptReserved_Creature);
	static auto NewIndexError = NewIndexErrorMaker(ScriptCreature, ScriptReserved_Creature);

	void ScriptCreature::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			ScriptCreature(const Moveable& mov)>;

		// Register type.
		parent.new_usertype<ScriptCreature>(ScriptReserved_Creature,
			ctors(), sol::call_constructor, ctors(),
			sol::meta_function::index, IndexError,
			sol::meta_function::new_index, NewIndexError,

			// Getters
			ScriptReserved_GetMood, &ScriptCreature::GetMood,
			ScriptReserved_GetCreatureTarget, &ScriptCreature::GetTarget,
			ScriptReserved_GetTargetPosition, &ScriptCreature::GetTargetPosition,
			ScriptReserved_GetAlerted, &ScriptCreature::GetAlerted,
			ScriptReserved_GetFriendly, &ScriptCreature::GetFriendly,
			ScriptReserved_GetHurtByPlayer, &ScriptCreature::GetHurtByPlayer,
			ScriptReserved_GetPoisoned, &ScriptCreature::GetPoisoned,
			ScriptReserved_GetLocationAI, &ScriptCreature::GetLocationAI,
			ScriptReserved_GetAtGoal, &ScriptCreature::GetAtGoal,

			// Setters
			ScriptReserved_SetMood, &ScriptCreature::SetMood,
			ScriptReserved_SetCreatureTarget, &ScriptCreature::SetTarget,
			ScriptReserved_SetAlerted, &ScriptCreature::SetAlerted,
			ScriptReserved_SetFriendly, &ScriptCreature::SetFriendly,
			ScriptReserved_SetHurtByPlayer, &ScriptCreature::SetHurtByPlayer,
			ScriptReserved_SetPoisoned, &ScriptCreature::SetPoisoned,
			ScriptReserved_SetAtGoal, &ScriptCreature::SetAtGoal,
			ScriptReserved_SetLocationAI, &ScriptCreature::SetLocationAI,

			// Inquirers
			ScriptReserved_GetValid, &ScriptCreature::GetValid,
			ScriptReserved_GetJumping, &ScriptCreature::GetJumping,
			ScriptReserved_GetMonkeying, &ScriptCreature::GetMonkeying);
	}

	// Manages warnings for invalid creature/moveable pointers.
	bool ScriptCreature::TestCreature(int itemNumber, bool silent)
	{
		if (itemNumber <= NO_VALUE || itemNumber >= (int)g_Level.Items.size())
		{
			if (!silent)
				TENLog(fmt::format("Attempt to access creature with invalid item number {}.", itemNumber), LogLevel::Warning);

			return false;
		}

		auto* item = &g_Level.Items[itemNumber];

		if (!item->IsCreature())
		{
			if (!silent)
				TENLog(fmt::format("Item {} does not correspond to an active creature. Make sure the item is a creature and it was activated.", item->Name), LogLevel::Warning);

			return false;
		}

		return true;
	}

	// Resolves the Creature pointer from the stored item number.
	// Returns nullptr if the item is invalid, inactive, or not a creature.
	CreatureInfo* ScriptCreature::GetCreature(bool silent) const
	{
		if (!TestCreature(_itemNumber, silent))
			return nullptr;

		auto* item = &g_Level.Items[_itemNumber];

		int sourceHash = GetHash(item->Name);
		if (sourceHash != _hash)
		{
			if (!silent)
				TENLog(fmt::format("Item {} has a name hash mismatch to this creature. Expected: {}, Actual: {}.", item->Name, _hash, sourceHash), LogLevel::Warning);
			
			return false;
		}

		return GetCreatureInfo(item);
	}

	/// Create creature info for the provided moveable.
	// @function Creature
	// @tparam Objects.Moveable moveable Moveable object to fetch creature from. Must be an active enemy.
	// @treturn Creature Creature info for the moveable.
	ScriptCreature::ScriptCreature(const Moveable& mov)
	{
		auto index = mov.GetIndex();
		if (!TestCreature(index))
			return;

		_itemNumber = mov.GetIndex();
		_hash = GetHash(mov.GetName());
	}

	/// Gets the current mood of the creature.
	// @function GetMood
	// @treturn Objects.MoodType The current mood of the creature. If creature is invalid, returns `nil`.
	std::optional<ScriptMoodType> ScriptCreature::GetMood()
	{
		auto* creature = GetCreature();
		if (creature == nullptr)
			return std::nullopt;

		return (ScriptMoodType)creature->Mood;
	}

	/// Sets the mood of the creature.
	// Overrides the automatic mood management and forces the creature's mood to the specified value.
	// Setting the mood to @{Objects.MoodType.AUTO} will clear any mood override and allow the creature to manage the mood according to the AI.
	// @function SetMood
	// @tparam Objects.MoodType mood The mood to set.
	void ScriptCreature::SetMood(ScriptMoodType mood)
	{
		auto* creature = GetCreature();
		if (creature == nullptr)
			return;

		if (mood == ScriptMoodType::Auto)
			creature->ForcedMood = std::nullopt;
		else
		{
			creature->ForcedMood = (MoodType)mood;
			creature->Mood = creature->ForcedMood.value();
		}
	}

	/// Gets the current target of the creature.
	// @function GetTarget
	// @treturn Objects.Moveable The moveable object representing the target. If creature is invalid or target is not set, returns `nil`.
	std::optional<Moveable> ScriptCreature::GetTarget()
	{
		auto* creature = GetCreature();
		if (creature == nullptr)
			return std::nullopt;

		auto* enemy = creature->Enemy.Get();

		if (enemy)
		{
			if (enemy->Index > NO_VALUE && enemy->Index < (int)g_Level.Items.size())
			{
				return Moveable(enemy->Index);
			}
			else
			{
				TENLog(fmt::format("Creature {} has invalid target pointer.", g_Level.Items[_itemNumber].Name), LogLevel::Warning);
			}
		}

		return std::nullopt;
	}

	/// Sets a new target for the creature.
	// @function SetTarget
	// @tparam Objects.Moveable mov The moveable object to set as the target. Set to `nil` to clear the target.
	void ScriptCreature::SetTarget(const TypeOrNil<Moveable*> moveable)
	{
		if (!IsValidOptional(moveable))
		{
			TENLog("Attempt to set creature target with invalid argument.", LogLevel::Warning);
			return;
		}

		auto* creature = GetCreature();
		if (creature == nullptr)
			return;

		if (std::holds_alternative<Moveable*>(moveable))
		{
			auto* targetMov = std::get<Moveable*>(moveable);

			if (targetMov == nullptr || !targetMov->GetValid())
			{
				TENLog(fmt::format("Attempt to set target for {} to an invalid moveable.", g_Level.Items[_itemNumber].Name), LogLevel::Warning);
				creature->Enemy = nullptr;
				return;
			}

			auto targetIndex = targetMov->GetIndex();
			if (targetIndex <= NO_VALUE || targetIndex >= (int)g_Level.Items.size())
			{
				TENLog(fmt::format("Attempt to set creature target for {} with invalid moveable index {}.", g_Level.Items[_itemNumber].Name, targetIndex), LogLevel::Warning);
				creature->Enemy = nullptr;
				return;
			}

			creature->Enemy = &g_Level.Items[targetIndex];
		}
		else
		{
			creature->Enemy = nullptr;
		}
	}

	/// Gets the current target position of the creature.
	// Target position may differ from the actual enemy position if @{Flow.Settings.Pathfinding.predictionFactor} is set.
	// If no enemy is currently set or mood is set to bored or stalk, returns the position where the creature is currently heading to.
	// @function GetTargetPosition
	// @treturn Vec3 The position of the creature's target. If creature is invalid, returns `nil`.
	std::optional<Vec3> ScriptCreature::GetTargetPosition()
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			return creature->Target;
		else
			return std::nullopt;
	}

	/// Gets the creature's alerted state.
	// @function GetAlerted
	// @treturn bool `true` if creature is alerted, `false` if not alerted. If creature is invalid, returns `nil`.
	std::optional<bool> ScriptCreature::GetAlerted()
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			return creature->Alerted;
		else
			return std::nullopt;
	}

	/// Sets the creature's alerted state.
	// @function SetAlerted
	// @tparam bool enabled `true` sets creature as alerted, `false` clears the alert state.
	void ScriptCreature::SetAlerted(bool enabled)
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			creature->Alerted = enabled;
	}

	/// Gets the creature's friendly state.
	// If creature was attacked by player, friendly state alone is not enough to know whether a creature is violent. For such cases,
	// @{Objects.Creature.GetHurtByPlayer} must also be used in combination with this method.
	// @function GetFriendly
	// @treturn bool `true` if the creature is marked as friendly, `false` if it is marked as not friendly. If creature is invalid, returns `nil`.
	std::optional<bool> ScriptCreature::GetFriendly()
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			return creature->Friendly;
		else
			return std::nullopt;
	}

	/// Sets the creature's friendly state.
	// Friendly creatures will not attack the player unless player attacks them first, except special cases when behavior is hardcoded.
	// If a friendly creature was attacked by player, @{Objects.Creature.SetHurtByPlayer} must be set to `false` as well to stop them attacking player.
	// @function SetFriendly
	// @tparam bool enabled `true` sets creature as friendly, `false` sets it as hostile.
	void ScriptCreature::SetFriendly(bool enabled)
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			creature->Friendly = enabled;
	}

	/// Gets whether the creature has been hurt by the player.
	// @function GetHurtByPlayer
	// @treturn bool `true` means that creature was hurt by player, `false` means it was not hurt by player. If creature is invalid, returns `nil`.
	std::optional<bool> ScriptCreature::GetHurtByPlayer()
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			return creature->HurtByLara;
		else
			return std::nullopt;
	}

	/// Sets whether the creature has been hurt by the player. This flag influences
	// creature mood toward escape behavior.
	// @function SetHurtByPlayer
	// @tparam bool enabled `true` marks the creature as hurt by player, `false` clears hurt state.
	void ScriptCreature::SetHurtByPlayer(bool enabled)
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			creature->HurtByLara = enabled;
	}

	/// Gets the creature's poisoned state.
	// @function GetPoisoned
	// @treturn bool `true` means the creature is poisoned, `false` means it's not poisoned. If creature is invalid, returns `nil`.
	std::optional<bool> ScriptCreature::GetPoisoned()
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			return creature->Poisoned;
		else
			return std::nullopt;
	}

	/// Sets the creature's poisoned state.
	// Poisoned creatures take periodic damage and may be slowly killed, if @{Flow.Settings.Gameplay.killPoisonedEnemies} setting is on.
	// @function SetPoisoned
	// @tparam bool enabled `true` sets creature as poisoned, `false` clears poisoned state.
	void ScriptCreature::SetPoisoned(bool enabled)
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			creature->Poisoned = enabled;
	}

	/// Gets whether the creature has reached its goal.
	// This setting may be used to find out whether a creature that is using AI nullmesh objects has reached its currently specified AI nullmesh.
	// @function GetAtGoal
	// @treturn bool `true` means the creature has reached the goal, `false` means it's not at goal. If creature is invalid, returns `nil`.
	std::optional<bool> ScriptCreature::GetAtGoal()
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			return creature->ReachedGoal;
		else
			return std::nullopt;
	}

	/// Sets whether the creature has reached its goal.
	// This setting may be used to break out the creature from reaching the next specified AI object nullmesh.
	// @function SetAtGoal
	// @tparam bool enabled `true` marks the creature as having reached the goal, `false` clears goal reached state.
	void ScriptCreature::SetAtGoal(bool enabled)
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			creature->ReachedGoal = enabled;
	}

	/// Get the OCB of the AI object that the enemy is currently trying to reach.
	// Used by specific enemies for custom waypoint logic:
	// 
	// - @{Objects.ObjID.SOPHIA_LEIGH_BOSS}
	// - @{Objects.ObjID.VON_CROY}
	// - @{Objects.ObjID.GUIDE}, only if he has ItemFlags[2] bit 1 set.
	// @function GetLocationAI
	// @treturn int The current AI location index. If creature is invalid, returns `nil`.
	std::optional<int> ScriptCreature::GetLocationAI()
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			return (int)creature->LocationAI;
		else
			return std::nullopt;
	}

	/// Updates the AI object OCB that the creature should try to reach.
	// Used by specific enemies for custom waypoint logic:
	// 
	// - @{Objects.ObjID.SOPHIA_LEIGH_BOSS}
	// - @{Objects.ObjID.VON_CROY}
	// - @{Objects.ObjID.GUIDE}, only if he has ItemFlags[2] bit 1 set (otherwise, he ignores it and simply look for the next AI object OCB until he reaches the one set by the last call to flipeffect 30).
	// @function SetLocationAI
	// @tparam int value The AI location index to set.
	void ScriptCreature::SetLocationAI(int value)
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			creature->LocationAI = (short)value;
	}

	/// Gets whether the creature's pathfinding currently involves a jump.
	// Only works for enemies that support jumping.
	// @function GetJumping
	// @treturn bool `true` if creature is jumping or about to jump, `false` if not jumping. If creature is invalid, returns `nil`.
	std::optional<bool> ScriptCreature::GetJumping()
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			return creature->LOT.IsJumping;
		else
			return std::nullopt;
	}

	/// Gets whether the creature's pathfinding currently involves monkey-swinging.
	// @function GetMonkeying
	// @treturn bool `true` if creature is monkey-swinging, `false` if not. If creature is invalid, returns `nil`.
	std::optional<bool> ScriptCreature::GetMonkeying()
	{
		auto* creature = GetCreature();
		if (creature != nullptr)
			return creature->LOT.IsMonkeying;
		else
			return std::nullopt;
	}

	/// Checks if the underlying creature data is still valid. Returns `false` if the creature was killed or disabled.
	// @function GetValid
	// @treturn bool `true` if creature data is valid, `false` if creature was killed or disabled.
	bool ScriptCreature::GetValid()
	{
		return GetCreature(true) != nullptr;
	}
}
