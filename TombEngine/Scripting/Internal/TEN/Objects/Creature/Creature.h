#pragma once
#include "framework.h"
#include "Game/itemdata/creature_info.h"

#include "Scripting/Internal/TEN/Objects/Creature/CreatureStates.h"
#include "Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Scripting/Internal/ScriptUtil.h"

namespace sol { class state; };

namespace TEN::Scripting::Objects
{
	class ScriptCreature
	{
	private:
		int _itemNumber = NO_VALUE;
		int _hash = 0;

		CreatureInfo* GetCreature(bool silent = false) const;
		static bool TestCreature(int itemNumber, bool silent = false);

	public:
		static void Register(sol::table& parent);

		// Constructors
		ScriptCreature() = default;
		ScriptCreature(const Moveable& mov);

		// Getters
		std::optional<ScriptMoodType> GetMood();
		std::optional<Moveable>	GetTarget();
		std::optional<Vec3> GetTargetPosition();
		std::optional<int>	GetLocationAI();
		std::optional<bool> GetAlerted();
		std::optional<bool> GetFriendly();
		std::optional<bool> GetHurtByPlayer();
		std::optional<bool> GetPoisoned();
		std::optional<bool> GetAtGoal();

		// Setters
		void SetMood(ScriptMoodType mood);
		void SetTarget(const TypeOrNil<Moveable*> moveable);
		void SetLocationAI(int value);
		void SetAlerted(bool enabled);
		void SetFriendly(bool enabled);
		void SetHurtByPlayer(bool enabled);
		void SetPoisoned(bool enabled);
		void SetAtGoal(bool enabled);

		// Inquirers
		bool GetValid();
		std::optional<bool> GetJumping();
		std::optional<bool> GetMonkeying();
	};
}
