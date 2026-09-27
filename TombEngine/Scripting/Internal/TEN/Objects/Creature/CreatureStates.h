#pragma once

#include "Game/itemdata/creature_info.h"
#include <unordered_map>
#include <string>

/// Constants for creature mood.
// To be used with @{Objects.Creature.GetMood} and @{Objects.Creature.SetMood} functions.
// @enum Objects.MoodType
// @pragma nostrip

namespace TEN::Scripting::Objects
{
	enum class ScriptMoodType
	{
		Bored,
		Attack,
		Escape,
		Stalk,
		Auto
	};

	static const auto MOOD_TYPES = std::unordered_map<std::string, ScriptMoodType>
	{
		/// Creature is not attacking or stalking any enemies and randomly roams around the area.
		// @mem BORED
		{"BORED", ScriptMoodType::Bored},

		/// Creature is searching for an enemy, but does not directly attack it yet.
		// @mem STALK
		{"STALK", ScriptMoodType::Stalk},

		/// Creature is attacking an enemy.
		// @mem ATTACK
		{"ATTACK", ScriptMoodType::Attack},

		/// Creature is escaping from the enemy and trying to find a safe place.
		// @mem ESCAPE
		{"ESCAPE", ScriptMoodType::Escape},

		/// Creature's mood is determined automatically by the AI.
		// @mem AUTO
		{"AUTO", ScriptMoodType::Auto}
	};
}
