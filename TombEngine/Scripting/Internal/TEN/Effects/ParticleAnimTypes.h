#pragma once

#include "Game/effects/effects.h"

namespace TEN::Scripting::Effects
{
	/// Constants for particle animation type constants. To be used with @{Effects.EmitAdvancedParticle} function.
	// @enum Effects.ParticleAnimationType
	// @pragma nostrip

	/// Table of Effects.ParticleAnimationType type constants. To be used with particles.
	//
	// - `LOOP` - Frames loop sequentially.
	// - `ONE_SHOT` - Frames play once and freeze on the last frame.
	// - `BACK_AND_FORTH` - Frames bounce back and forth.
	// - `LIFE_TIME_SPREAD` - Frames are distributed over the particle's lifetime
	//
	// @table Effects.ParticleAnimationType

	static const auto PARTICLE_ANIM_TYPES = std::unordered_map<std::string, ParticleAnimType>
	{
		{ "LOOP", ParticleAnimType::Loop },
		{ "ONE_SHOT", ParticleAnimType::OneShot },
		{ "BACK_AND_FORTH", ParticleAnimType::BackAndForth },
		{ "LIFE_TIME_SPREAD", ParticleAnimType::LifetimeSpread } // TODO: Rename to LIFETIME_SPREAD.
	};
}
