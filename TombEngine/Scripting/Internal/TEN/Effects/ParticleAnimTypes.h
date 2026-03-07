#pragma once

#include "Game/effects/effects.h"

namespace TEN::Scripting::Effects
{
	/// Constants for particle animation type constants.
	// To be used with @{Effects.EmitAdvancedParticle} function.
	// @enum Effects.ParticleAnimationType
	// @pragma nostrip

	static const auto PARTICLE_ANIM_TYPES = std::unordered_map<std::string, ParticleAnimType>
	{
		/// Frames loop sequentially.
		// @mem LOOP
		{ "LOOP", ParticleAnimType::Loop },

		/// Frames play once and freeze on the last frame.
		// @mem ONE_SHOT
		{ "ONE_SHOT", ParticleAnimType::OneShot },

		/// Frames bounce back and forth.
		// @mem BACK_AND_FORTH
		{ "BACK_AND_FORTH", ParticleAnimType::BackAndForth },

		/// Frames are distributed over the particle's lifetime.
		// @mem LIFE_TIME_SPREAD
		{ "LIFE_TIME_SPREAD", ParticleAnimType::LifetimeSpread } // TODO: Rename to LIFETIME_SPREAD.
	};
}
