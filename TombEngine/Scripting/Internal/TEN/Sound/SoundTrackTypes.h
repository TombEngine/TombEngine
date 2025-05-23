#pragma once
#include "Sound/sound.h"

/***
Constants for the type of the audio tracks.
@enum Sound.SoundTrackType
@pragma nostrip
*/

/*** Table of Sound.SoundTrackType constants.
* 
To be used with sound track functions, such as @{Sound.PlayAudioTrack} and @{Sound.StopAudioTrack}.

 - `ONESHOT` - Used for one-time music tracks.
 - `LOOPED` - Used for looped ambience or music.
 - `VOICE` - Used for dialogs. Also supports subtitles, set by @{Sound.GetCurrentSubtitle} function.

@table Sound.SoundTrackType
*/

static const std::unordered_map<std::string, SoundTrackType> SOUNDTRACK_TYPE
{
	{ "ONESHOT", SoundTrackType::OneShot },
	{ "LOOPED", SoundTrackType::BGM },
	{ "VOICE", SoundTrackType::Voice }
};
