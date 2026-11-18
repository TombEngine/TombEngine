#include "framework.h"
#include "Scripting/Internal/TEN/Sound/SoundHandler.h"

#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Sound/AudioChannel.h"
#include "Scripting/Internal/TEN/Sound/SoundTrackTypes.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Sound/sound.h"

/// Functions for sound management.
// @tentable Sound
// @pragma nostrip

namespace TEN::Scripting::Sound
{
	// COMPATIBILITY. Do not restore the documentation for this function.
	static void PlayAudioTrack(const std::string& trackName, TypeOrNil<SoundTrackType> mode)
	{
		auto playMode = ValueOr<SoundTrackType>(mode, SoundTrackType::OneShot);
		PlaySoundTrack(trackName, playMode);
	}

	// COMPATIBILITY. Do not restore the documentation for this function.
	static void SetAmbientTrack(const std::string& trackName, TypeOrNil<bool> fromTheBeginning)
	{
		auto pos = ValueOr<bool>(fromTheBeginning, false) ? std::optional<QWORD>(0) : std::optional<QWORD>();
		PlaySoundTrack(trackName, SoundTrackType::BGM, pos, pos.has_value() ? SOUND_XFADETIME_ONESHOT : SOUND_XFADETIME_BGM);
	}

	// COMPATIBILITY. Do not restore the documentation for this function.
	static void StopAudioTracks()
	{
		StopSoundTracks();
	}

	// COMPATIBILITY. Do not restore the documentation for this function.
	static void StopAudioTrack(TypeOrNil<SoundTrackType> mode)
	{
		auto playMode = ValueOr<SoundTrackType>(mode, SoundTrackType::OneShot);
		StopSoundTrack(playMode, SOUND_XFADETIME_ONESHOT);
	}

	// COMPATIBILITY. Do not restore the documentation for this function.
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
		if (pos.has_value())
		{
			auto soundPose = Pose(pos->ToVector3i());
			SoundEffect(soundID, &soundPose, SoundEnvironment::Always);
		}
		else
		{
			SoundEffect(soundID, nullptr, SoundEnvironment::Always);
		}
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

	// COMPATIBILITY. Do not restore the documentation for this function.
	static bool IsAudioTrackPlaying(const std::string& trackName, sol::optional<SoundTrackType> type)
	{
		return Sound_TrackIsPlaying(trackName, type ? std::optional<SoundTrackType>(*type) : std::nullopt);
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

		AudioChannel::Register(*state, tableSound);
	}
};
