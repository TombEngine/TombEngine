#pragma once

#include <string>
#include <vector>

namespace TEN::Sound
{
	// Structure representing a serialized Lua soundtrack state.
	struct SoundtrackSaveData
	{
		std::string Name;
		unsigned long long Position = 0;
		float Volume = 1.0f;
		int PlayMode = 0;
		bool IsPaused = false;
		bool Loop = false;
		bool HasLoopOverride = false;
		int FadeInTime = 0;
		int FadeOutTime = 0;
		int CrossfadeTime = 0;
		bool ShuffleStart = false;
		bool DampenAmbient = true;
		bool RestoreAmbient = true;
	};

	// Save all persistent Lua-managed soundtracks.
	std::vector<SoundtrackSaveData> SaveLuaSoundtracks();

	// Restore Lua-managed soundtracks from saved data.
	void LoadLuaSoundtracks(const std::vector<SoundtrackSaveData>& data);

	// Clear all Lua-managed soundtracks (called on level change or new game).
	void ClearLuaSoundtracks();
}
