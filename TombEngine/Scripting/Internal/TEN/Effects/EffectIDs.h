#pragma once

#include "Game/items.h"

namespace TEN::Scripting::Effects
{
	/// Constants for effect IDs.
	// @enum Effects.EffectID
	// @pragma nostrip

	/// Table of Effects.EffectID constants.
	// To be used with @{Objects.Moveable.SetEffect} and @{Objects.Moveable.GetEffect} functions.
	//
	// - `NONE`
	// - `FIRE`
	// - `SPARKS`
	// - `SMOKE`
	// - `ELECTRIC_IGNITE`
	// - `RED_IGNITE`
	// - `CADAVER`
	// - `CUSTOM`
	//
	// @table Effects.EffectID

	static const auto EFFECT_IDS = std::unordered_map<std::string, EffectType>
	{
		{ "NONE", EffectType::None },
		{ "FIRE", EffectType::Fire },
		{ "SPARKS", EffectType::Sparks },
		{ "SMOKE", EffectType::Smoke},
		{ "ELECTRIC_IGNITE", EffectType::ElectricIgnite },
		{ "RED_IGNITE", EffectType::RedIgnite },
		{ "CUSTOM", EffectType::Custom },

		// COMPATIBILITY
		{ "ELECTRICIGNITE", EffectType::ElectricIgnite },
		{ "REDIGNITE", EffectType::RedIgnite }
	};
}

