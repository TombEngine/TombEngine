#pragma once
#include "Sound/sound.h"

/// Constants for the type of the audio tracks.
// To be used with sound track functions, such as @{Sound.PlayAudioTrack} and @{Sound.StopAudioTrack}.
// @enum Sound.SoundTrackType
// @pragma nostrip

static const std::unordered_map<std::string, SoundTrackType> SOUNDTRACK_TYPE
{
	/// Used for one-time music tracks.
	// @mem ONESHOT
	{ "ONESHOT", SoundTrackType::OneShot },

	/// Used for looped ambience or music.
	// @mem LOOPED
	{ "LOOPED", SoundTrackType::BGM },

	/// Used for dialogs. Also supports subtitles, set by @{Sound.GetCurrentSubtitle} function.
	// @mem VOICE
	{ "VOICE", SoundTrackType::Voice }
};
