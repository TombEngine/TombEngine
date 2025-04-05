#include "framework.h"
#include "Scripting/Internal/TEN/Sound/SoundHandler.h"

#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Sound/SoundTrackTypes.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Sound/sound.h"

/// Functions for sound management.
// @tentable Sound
// @pragma nostrip

namespace TEN::Scripting::Sound
{
	/// Play an audio track. Should be placed in the `Audio` folder. Supported formats are wav, mp3 and ogg.
	// @function PlayAudioTrack
	// @tparam string filename Filename of a track (without file extension) to play.
	// @tparam Sound.SoundTrackType type Type of the audio track to play.
	static void PlayAudioTrack(const std::string& trackName, TypeOrNil<SoundTrackType> mode)
	{
		auto playMode = ValueOr<SoundTrackType>(mode, SoundTrackType::OneShot);
		PlaySoundTrack(trackName, playMode);
	}

	/// Set and play an ambient track.
	// @function SetAmbientTrack
	// @tparam string name Name of track (without file extension) to play.
	// @tparam bool fromStart Specifies whether ambient track should play from the start, or crossfade at a random position.
	static void SetAmbientTrack(const std::string& trackName, TypeOrNil<bool> fromTheBeginning)
	{
		auto pos = ValueOr<bool>(fromTheBeginning, false) ? std::optional<QWORD>(0) : std::optional<QWORD>();
		PlaySoundTrack(trackName, SoundTrackType::BGM, pos, pos.has_value() ? SOUND_XFADETIME_ONESHOT : SOUND_XFADETIME_BGM);
	}

	/// Stop any audio tracks currently playing.
	// @function StopAudioTracks
	static void StopAudioTracks()
	{
		StopSoundTracks();
	}

	/// Stop audio track that is currently playing.
	// @function StopAudioTrack
	// @tparam Sound.SoundTrackType type Type of the audio track.
	static void StopAudioTrack(TypeOrNil<SoundTrackType> mode)
	{
		auto playMode = ValueOr<SoundTrackType>(mode, SoundTrackType::OneShot);
		StopSoundTrack(playMode, SOUND_XFADETIME_ONESHOT);
	}

	/// Get current loudness level for specified track type.
	// @function GetAudioTrackLoudness
	// @tparam Sound.SoundTrackType type Type of the audio track.
	// @treturn float Current loudness of a specified audio track.
	static float GetAudioTrackLoudness(TypeOrNil<SoundTrackType> mode)
	{
		auto playMode = ValueOr<SoundTrackType>(mode, SoundTrackType::OneShot);
		return GetSoundTrackLoudness(playMode);
	}

	/// Play sound effect.
	// @function PlaySound
	// @tparam int soundID Sound ID to play. Corresponds to the value in the sound XML file or Tomb Editor's "Sound Infos" window.
	// @tparam[opt] Vec3 position The 3D position of the sound, i.e. where the sound "comes from". If not given, the sound will not be positional.
	static void PlaySoundEffect(int soundID, sol::optional<Vec3> pos)
	{
		SoundEffect(soundID, pos.has_value() ? &Pose(pos->ToVector3i()) : nullptr, SoundEnvironment::Always);
	}

	/// Stop sound effect.
	// @function StopSound
	// @tparam int soundID Sound ID to play. Corresponds to the value in the sound XML file or Tomb Editor's "Sound Infos" window.
	static void StopSound(int id)
	{
		StopSoundEffect(id);
	}

	/// Check if the sound effect is playing.
	// @function IsSoundPlaying
	// @tparam int soundID Sound ID to check. Corresponds to the value in the sound XML file or Tomb Editor's "Sound Infos" window.
	static bool IsSoundPlaying(int effectID)
	{
		return (Sound_EffectIsPlaying(effectID, nullptr) != SOUND_NO_CHANNEL);
	}

	/// Check if the audio track is playing.
	// @function IsAudioTrackPlaying
	// @tparam string Track Filename to check. Should be without extension and without full directory path.
	static bool IsAudioTrackPlaying(const std::string& trackName)
	{
		return Sound_TrackIsPlaying(trackName);
	}

	/// Get current subtitle string for a voice track currently playing.
	// Subtitle file must be in .srt format, have same filename as voice track, and be placed in same directory as voice track.
	// Returns nil if no voice track is playing or no subtitle present.
	// @function GetCurrentSubtitle
	// @treturn string Current subtitle string.
	static TypeOrNil<std::string> GetCurrentVoiceTrackSubtitle()
	{
		auto& result = GetCurrentSubtitle();

		if (result.has_value())
		{
			return result.value();
		}
		else
		{
			return sol::nil;
		}
	}

	void Register(sol::state* state, sol::table& parent)
	{
		sol::table tableSound{ state->lua_state(), sol::create };
		parent.set(ScriptReserved_Sound, tableSound);

		tableSound.set_function(ScriptReserved_PlayAudioTrack, &PlayAudioTrack);
		tableSound.set_function(ScriptReserved_SetAmbientTrack, &SetAmbientTrack);
		tableSound.set_function(ScriptReserved_StopAudioTracks, &StopAudioTracks);
		tableSound.set_function(ScriptReserved_StopAudioTrack, &StopAudioTrack);
		tableSound.set_function(ScriptReserved_GetAudioTrackLoudness, &GetAudioTrackLoudness);
		tableSound.set_function(ScriptReserved_PlaySound, &PlaySoundEffect);
		tableSound.set_function(ScriptReserved_StopSound, &StopSound);
		tableSound.set_function(ScriptReserved_IsSoundPlaying, &IsSoundPlaying);
		tableSound.set_function(ScriptReserved_IsAudioTrackPlaying, &IsAudioTrackPlaying);
		tableSound.set_function(ScriptReserved_GetCurrentSubtitle, &GetCurrentVoiceTrackSubtitle);

		LuaHandler handler{ state };
		handler.MakeReadOnlyTable(tableSound, ScriptReserved_SoundTrackType, SOUNDTRACK_TYPE);
	}
};
