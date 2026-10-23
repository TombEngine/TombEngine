#pragma once

#include "Sound/sound.h"

/// Constants for the playback mode of sound effects.
// To be used with @{Sound.PlaySound} and @{Objects.SoundSource:SetProperties}.
// @enum Sound.SoundPlayMode
// @pragma nostrip

static const std::unordered_map<std::string, SoundPlayMode> SOUND_PLAY_MODES
{
	/// Play the sound normally.
	// @mem NORMAL
	{ "NORMAL", SoundPlayMode::Normal },

	/// Wait for any currently playing instance to stop before playing.
	// @mem WAIT
	{ "WAIT", SoundPlayMode::Wait },

	/// Stop any currently playing instance and restart the sound.
	// @mem RESTART
	{ "RESTART", SoundPlayMode::Restart },

	/// Play the sound looped.
	// @mem LOOPED
	{ "LOOPED", SoundPlayMode::Looped }
};
