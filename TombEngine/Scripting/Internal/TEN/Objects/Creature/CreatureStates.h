#pragma once

#include "Game/itemdata/creature_info.h"
#include <unordered_map>
#include <string>

/// Constants for creature mood.
// @enum Objects.CreatureMood
// @pragma nostrip

/// Table of Objects.Creature constants.
// To be used with @{Objects.CreatureInfo.GetMood} function.
//
// - `BORED`
// - `ATTACK`
// - `ESCAPE`
// - `STALK`
//
// @table Objects.CreatureMood

namespace TEN::Scripting::Objects
{
	static const auto CREATURE_MOOD = std::unordered_map<std::string, MoodType>
	{
       
        {"BORED", MoodType::Bored},
        {"ATTACK", MoodType::Attack},
        {"ESCAPE", MoodType::Escape},
        {"STALK", MoodType::Stalk}
	};
}
