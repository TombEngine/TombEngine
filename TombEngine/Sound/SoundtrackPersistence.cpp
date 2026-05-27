#include "framework.h"
#include "Sound/SoundtrackPersistence.h"

#include "Sound/SoundtrackObject.h"
#include "Sound/SoundtrackRegistry.h"

namespace TEN::Sound
{
	std::vector<SoundtrackSaveData> SaveLuaSoundtracks()
	{
		std::vector<SoundtrackSaveData> result;

		auto persistentTracks = GetSoundtrackRegistry().GetPersistentSoundtracks();
		for (auto& track : persistentTracks)
		{
			SoundtrackSaveData data;
			data.Name = track->GetTrackName();
			data.Position = track->GetBytePosition();
			data.Volume = track->GetVolume();
			data.PlayMode = (int)track->GetPlayMode();
			data.IsPaused = track->IsPaused();

			auto& options = track->GetOptions();
			data.Loop = options.Loop.value_or(false);
			data.HasLoopOverride = options.Loop.has_value();
			data.FadeInTime = options.FadeInTime;
			data.FadeOutTime = options.FadeOutTime;
			data.CrossfadeTime = options.CrossfadeTime;
			data.ShuffleStart = options.ShuffleStart;
			data.DampenAmbient = options.DampenAmbient;
			data.RestoreAmbient = options.RestoreAmbient;

			result.push_back(data);
		}

		return result;
	}

	void LoadLuaSoundtracks(const std::vector<SoundtrackSaveData>& data)
	{
		// Clear existing Lua soundtracks before restoring.
		GetSoundtrackRegistry().Clear();

		for (auto& saved : data)
		{
			SoundtrackOptions options;
			options.Volume = saved.Volume;
			options.PlayMode = (SoundTrackType)saved.PlayMode;
			options.FadeInTime = saved.FadeInTime;
			options.FadeOutTime = saved.FadeOutTime;
			options.CrossfadeTime = saved.CrossfadeTime;
			options.ShuffleStart = saved.ShuffleStart;
			options.DampenAmbient = saved.DampenAmbient;
			options.RestoreAmbient = saved.RestoreAmbient;
			options.PersistInSave = true;
			options.StartPosition = saved.Position;

			if (saved.HasLoopOverride)
				options.Loop = saved.Loop;

			auto soundtrack = GetSoundtrackRegistry().GetOrCreate(saved.Name, options);
			if (soundtrack)
			{
				if (saved.IsPaused)
				{
					// Start playing then immediately pause to restore paused state.
					soundtrack->Play();
					soundtrack->Pause();
				}
				else
				{
					// Resume playing from saved position.
					soundtrack->Play();
				}
			}
		}
	}

	void ClearLuaSoundtracks()
	{
		GetSoundtrackRegistry().Clear();
	}
}
