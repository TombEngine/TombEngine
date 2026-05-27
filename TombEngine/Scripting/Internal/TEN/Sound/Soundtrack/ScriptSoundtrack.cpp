#include "framework.h"
#include "Scripting/Internal/TEN/Sound/Soundtrack/ScriptSoundtrack.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Sound/SoundTrackTypes.h"
#include "Sound/SoundtrackObject.h"
#include "Sound/SoundtrackRegistry.h"

using namespace TEN::Sound;

/// Object-based soundtrack for advanced music control.
//
// Provides a modern object-based API for soundtrack playback.
// Soundtrack objects are owned by the engine and persist across save/load.
//
// Usage:
//
//     local music = Sound.Soundtrack("ambient_caves", { volume = 0.8, playMode = Sound.SoundTrackType.LOOPED })
//     music:Play()
//     music:Pause()
//     music:SetVolume(0.5)
//
// @tenclass Sound.Soundtrack
// @pragma nostrip

namespace TEN::Scripting::Sound
{
	// Helper to parse the options table from Lua.
	static SoundtrackOptions ParseOptions(sol::optional<sol::table> optTable)
	{
		SoundtrackOptions options;

		if (!optTable.has_value())
			return options;

		auto& table = optTable.value();

		if (auto vol = table.get<sol::optional<float>>("volume"))
			options.Volume = std::clamp(vol.value(), 0.0f, 1.0f);

		if (auto mode = table.get<sol::optional<SoundTrackType>>("playMode"))
			options.PlayMode = mode.value();

		if (auto fadeIn = table.get<sol::optional<int>>("fadeInTime"))
			options.FadeInTime = std::max(0, fadeIn.value());

		if (auto fadeOut = table.get<sol::optional<int>>("fadeOutTime"))
			options.FadeOutTime = std::max(0, fadeOut.value());

		if (auto crossfade = table.get<sol::optional<int>>("crossfadeTime"))
			options.CrossfadeTime = std::max(0, crossfade.value());

		if (auto loop = table.get<sol::optional<bool>>("loop"))
			options.Loop = loop.value();

		if (auto shuffle = table.get<sol::optional<bool>>("shuffleStart"))
			options.ShuffleStart = shuffle.value();

		if (auto dampen = table.get<sol::optional<bool>>("dampenAmbient"))
			options.DampenAmbient = dampen.value();

		if (auto restore = table.get<sol::optional<bool>>("restoreAmbient"))
			options.RestoreAmbient = restore.value();

		if (auto persist = table.get<sol::optional<bool>>("persistInSave"))
			options.PersistInSave = persist.value();

		if (auto autoPlay = table.get<sol::optional<bool>>("autoPlay"))
			options.AutoPlay = autoPlay.value();

		return options;
	}

	// Factory function used as the Lua constructor.
	static std::shared_ptr<SoundtrackObject> CreateSoundtrack(const std::string& trackName, sol::optional<sol::table> optTable)
	{
		if (trackName.empty())
		{
			ScriptAssert(false, "Sound.Soundtrack: track name must not be empty.");
			return nullptr;
		}

		auto options = ParseOptions(optTable);
		return GetSoundtrackRegistry().GetOrCreate(trackName, options);
	}

	/// Stop the soundtrack.
	// @function Soundtrack:Stop
	// @tparam[opt] int fadeOutTime Optional fade-out time in milliseconds.
	static void Soundtrack_Stop(SoundtrackObject& self, sol::optional<int> fadeOutTime)
	{
		if (fadeOutTime.has_value())
			self.Stop(fadeOutTime.value());
		else
			self.Stop();
	}

	void RegisterSoundtrackType(sol::state* state, sol::table& soundTable)
	{
		// Register the Soundtrack usertype on the Sound table.
		soundTable.new_usertype<SoundtrackObject>(
			ScriptReserved_Soundtrack,
			sol::no_constructor,
			sol::call_constructor, sol::factories(&CreateSoundtrack),

			ScriptReserved_SoundtrackPlay, &SoundtrackObject::Play,
			ScriptReserved_SoundtrackPause, &SoundtrackObject::Pause,
			ScriptReserved_SoundtrackResume, &SoundtrackObject::Resume,
			ScriptReserved_SoundtrackStop, &Soundtrack_Stop,
			ScriptReserved_SoundtrackSetVolume, &SoundtrackObject::SetVolume,
			ScriptReserved_SoundtrackGetVolume, &SoundtrackObject::GetVolume,
			ScriptReserved_SoundtrackSetPlayMode, &SoundtrackObject::SetPlayMode,
			ScriptReserved_SoundtrackGetPlayMode, &SoundtrackObject::GetPlayMode,
			ScriptReserved_SoundtrackSetPosition, &SoundtrackObject::SetPosition,
			ScriptReserved_SoundtrackGetPosition, &SoundtrackObject::GetPosition,
			ScriptReserved_SoundtrackGetLength, &SoundtrackObject::GetLength,
			ScriptReserved_SoundtrackIsPlaying, &SoundtrackObject::IsPlaying,
			ScriptReserved_SoundtrackIsPaused, &SoundtrackObject::IsPaused,
			ScriptReserved_SoundtrackIsActive, &SoundtrackObject::IsActive,
			ScriptReserved_SoundtrackGetTrackName, &SoundtrackObject::GetTrackName,
			ScriptReserved_SoundtrackGetRuntimeID, &SoundtrackObject::GetRuntimeID
		);
	}
}
