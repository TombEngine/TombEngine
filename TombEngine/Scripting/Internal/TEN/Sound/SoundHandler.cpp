#include "framework.h"
#include "Scripting/Internal/TEN/Sound/SoundHandler.h"

#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Sound/SoundPlayModes.h"
#include "Scripting/Internal/TEN/Sound/SoundTrackTypes.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/level.h"
#include "Sound/sound.h"

/// Functions for sound management.
// @tentable Sound
// @pragma nostrip

namespace TEN::Scripting::Sound
{
	static constexpr char VolumeKey[]		= "volume";
	static constexpr char RadiusKey[]		= "radius";
	static constexpr char RandomnessKey[]	= "randomness";
	static constexpr char PitchKey[]		= "pitch";
	static constexpr char PlayModeKey[]		= "playMode";
	static constexpr char RandomPitchKey[]	= "randomPitch";
	static constexpr char RandomGainKey[]	= "randomGain";

	SoundProperties ParseSoundProperties(const sol::table& table)
	{
		auto properties = SoundProperties{};

		if (auto value = table.get<sol::optional<int>>(VolumeKey); value.has_value())
			properties.Volume = (unsigned char)std::clamp(value.value(), 0, UCHAR_MAX);

		if (auto value = table.get<sol::optional<int>>(RadiusKey); value.has_value())
			properties.Radius = (unsigned char)std::clamp(value.value(), 0, UCHAR_MAX);

		if (auto value = table.get<sol::optional<int>>(RandomnessKey); value.has_value())
			properties.Randomness = (unsigned char)std::clamp(value.value(), 0, UCHAR_MAX);

		if (auto value = table.get<sol::optional<int>>(PitchKey); value.has_value())
			properties.Pitch = (signed char)std::clamp(value.value(), -127, 127);

		if (auto value = table.get<sol::optional<int>>(PlayModeKey); value.has_value())
			properties.PlayMode = (SoundPlayMode)std::clamp(value.value(), 0, (int)SoundPlayMode::Looped);

		if (auto value = table.get<sol::optional<bool>>(RandomPitchKey); value.has_value())
			properties.RandomPitch = value.value();

		if (auto value = table.get<sol::optional<bool>>(RandomGainKey); value.has_value())
			properties.RandomGain = value.value();

		return properties;
	}

	SoundProperties GetEffectiveSoundProperties(const SoundSourceInfo& soundSource)
	{
		auto properties = soundSource.Properties;

		if (soundSource.SoundID < 0 || soundSource.SoundID >= (int)g_Level.SoundMap.size())
			return properties;

		int sampleIndex = g_Level.SoundMap[soundSource.SoundID];
		if (sampleIndex < 0 || sampleIndex >= (int)g_Level.SoundDetails.size())
			return properties;

		const auto& sample = g_Level.SoundDetails[sampleIndex];

		if (!properties.Volume.has_value())
			properties.Volume = sample.Volume;

		if (!properties.Radius.has_value())
			properties.Radius = sample.Radius;

		if (!properties.Randomness.has_value())
			properties.Randomness = sample.Randomness;

		if (!properties.Pitch.has_value())
			properties.Pitch = sample.Pitch;

		if (!properties.PlayMode.has_value())
			properties.PlayMode = (SoundPlayMode)(sample.Flags & 3);

		if (!properties.RandomPitch.has_value())
			properties.RandomPitch = (sample.Flags & SOUND_FLAG_RND_PITCH) != 0;

		if (!properties.RandomGain.has_value())
			properties.RandomGain = (sample.Flags & SOUND_FLAG_RND_GAIN) != 0;

		return properties;
	}

	sol::table MakeSoundPropertiesTable(sol::this_state state, const SoundProperties& properties)
	{
		auto table = sol::state_view(state).create_table();

		if (properties.Volume.has_value())
			table[VolumeKey] = (int)properties.Volume.value();

		if (properties.Radius.has_value())
			table[RadiusKey] = (int)properties.Radius.value();

		if (properties.Randomness.has_value())
			table[RandomnessKey] = (int)properties.Randomness.value();

		if (properties.Pitch.has_value())
			table[PitchKey] = (int)properties.Pitch.value();

		if (properties.PlayMode.has_value())
			table[PlayModeKey] = (int)properties.PlayMode.value();

		if (properties.RandomPitch.has_value())
			table[RandomPitchKey] = properties.RandomPitch.value();

		if (properties.RandomGain.has_value())
			table[RandomGainKey] = properties.RandomGain.value();

		return table;
	}

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
	// @tparam[opt] table properties Table of sound properties to override the catalogue values. Supported fields are `volume` (0-255), `radius` (0-255 sectors), `randomness` (0-255), `pitch` (-127-127), `playMode` (a @{Sound.SoundPlayMode} value), `randomPitch` (bool) and `randomGain` (bool).
	static void PlaySoundEffect(int soundID, sol::optional<Vec3> pos, sol::optional<sol::table> properties)
	{
		auto soundProperties = SoundProperties{};
		if (properties.has_value())
			soundProperties = ParseSoundProperties(properties.value());

		auto* soundPropertiesPtr = soundProperties.IsEmpty() ? nullptr : &soundProperties;

		if (pos.has_value())
		{
			auto soundPose = Pose(pos->ToVector3i());
			SoundEffect(soundID, &soundPose, SoundEnvironment::Always, 1.0f, 1.0f, soundPropertiesPtr);
		}
		else
		{
			SoundEffect(soundID, nullptr, SoundEnvironment::Always, 1.0f, 1.0f, soundPropertiesPtr);
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

	/// Check if the audio track is playing.
	// @function IsAudioTrackPlaying
	// @tparam string track Filename to check. Should be without extension and without full directory path.
	// @tparam[opt] Sound.SoundTrackType type Type of the audio track to check. If omitted, all types will be checked.
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
		handler.MakeReadOnlyTable(tableSound, ScriptReserved_SoundPlayMode, SOUND_PLAY_MODES);
	}
};
