#pragma once

#include "Game/items.h"

namespace TEN::Scripting::Effects
{
	/// Constants for effect IDs.
	// To be used with @{Objects.Moveable.SetEffect} and @{Objects.Moveable.GetEffect} functions.
	// @enum Effects.EffectID
	// @pragma nostrip

	static const auto EFFECT_IDS = std::unordered_map<std::string, EffectType>
	{
		/// No effect.
		// @mem NONE
		{ "NONE", EffectType::None },

		/// Fire effect.
		// @mem FIRE
		{ "FIRE", EffectType::Fire },

		/// Sparks effect.
		// @mem SPARKS
		{ "SPARKS", EffectType::Sparks },

		/// Smoke effect.
		// @mem SMOKE
		{ "SMOKE", EffectType::Smoke},

		/// Electric ignite effect.
		// @mem ELECTRIC_IGNITE
		{ "ELECTRIC_IGNITE", EffectType::ElectricIgnite },

		/// Red ignite effect.
		// @mem RED_IGNITE
		{ "RED_IGNITE", EffectType::RedIgnite },

		/// Custom effect.
		// @mem CUSTOM
		{ "CUSTOM", EffectType::Custom },

		// COMPATIBILITY
		{ "ELECTRICIGNITE", EffectType::ElectricIgnite },
		{ "REDIGNITE", EffectType::RedIgnite }
	};
}

