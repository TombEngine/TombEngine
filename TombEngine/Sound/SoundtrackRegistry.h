#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Sound/SoundtrackObject.h"

namespace TEN::Sound
{
	class SoundtrackRegistry
	{
	private:
		std::unordered_map<std::string, std::shared_ptr<SoundtrackObject>> _registry;

	public:
		// Get or create a soundtrack by name.
		// If a soundtrack with this name already exists, returns the existing one.
		std::shared_ptr<SoundtrackObject> GetOrCreate(const std::string& trackName, const SoundtrackOptions& options);

		// Get an existing soundtrack by name. Returns nullptr if not found.
		std::shared_ptr<SoundtrackObject> Get(const std::string& trackName) const;

		// Check if a soundtrack with this name exists.
		bool Contains(const std::string& trackName) const;

		// Remove a soundtrack from the registry.
		void Remove(const std::string& trackName);

		// Clear all registered soundtracks.
		void Clear();

		// Get all active soundtracks (for save/load).
		std::vector<std::shared_ptr<SoundtrackObject>> GetActiveSoundtracks() const;

		// Get all persistent soundtracks (for save).
		std::vector<std::shared_ptr<SoundtrackObject>> GetPersistentSoundtracks() const;
	};

	// Global registry instance.
	SoundtrackRegistry& GetSoundtrackRegistry();

	// Helper for accessing the full audio directory (exposes from sound.cpp).
	std::string GetFullAudioDirectory();
}
